//--------------------------------------------------------------------------
// Find duplicate files and hard link, delete, or write batch files to do the same.
// Also includes a separate option to scan for and enumerate hardlinks in the search space.
//
// Version 1.25
//
// Matthias Wandel Oct 2006 - Aug 2010
// MinGW Unicode port Jarek Jurasz March 2017
//--------------------------------------------------------------------------

#define VERSION _T("1.25")
#include "tchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#include <process.h>
#include <io.h>
#include <sys/utime.h>
#define WIN32_LEAN_AND_MEAN // To keep windows.h bloat down.
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <direct.h>

#define  S_IWUSR  0x80      // user has write permission
#define  S_IWGRP  0x10      // group has write permission
#define  S_IWOTH  0x02      // others have write permisson

static int FilesMatched;

typedef struct {
    unsigned int Crc;
    unsigned int Sum;
}Checksum_t;

// Data structure for file allcoations:
typedef struct {
    Checksum_t Checksum;
    struct {
        int High;
        int Low;
    }FileIndex;
    int NumLinks;
    unsigned FileSize;
    TCHAR * FileName;
    int Larger; // Child index for larger child
    int Smaller;// Child index for smaller child
}FileData_t;
static FileData_t * FileData;
static int NumAllocated;
static int NumUnique;

// Duplicate statistics summary
struct {
    int TotalFiles;
    int DuplicateFiles;
    int HardlinkGroups;
    int CantReadFiles;
    int ZeroLengthFiles;
    int IgnoredFiles;
    __int64 TotalBytes;
    __int64 DuplicateBytes;
}DupeStats;

// How many bytes to calculate file signature of.
#define BYTES_DO_CHECKSUM_OF 32768


// Parameters for what to do
FILE * BatchFile = NULL;        // Output a batch file
TCHAR * BatchFileName = NULL;

int PrintFileSigs;         // Print signatures of files
int PrintDuplicates;       // Print duplicates
int MakeHardLinks;         // Do the actual hard linking
int DelDuplicates;         // Delete duplicates (no hard linking)
int ReferenceFiles;        // Flag - do not touch present files parsed
int DoReadonly;            // Do it for readonly files also
int Verbose;
int HardlinkSearchMode;    // Detect hard links only (do not check duplicates)
int ShowProgress = 1;      // Show progressing file count...
int HideCantReadMessage= 0;// Hide the can't read file error
int SkipZeroLength = 1;    // Ignore zero length files.
int ProgressIndicatorVisible = 0; // Weither a progress indicator needs to be overwritten.
int FollowReparse = 0;     // Whether to follow reparse points (like unix softlinks for NTFS)
TCHAR* IgnPats[20];        // Substrings to skip (can be multiple - like .git, .svn, .bak)
int NumIgnPats = 0;        // Number of active skip substrings
extern int MaxDepth = 0;   // Max recursion depth (0 = inf)

#define NELEMS(arr) (sizeof(arr)/sizeof(*arr))


int MyGlob(const TCHAR * Pattern, int FollowReparse, void (*FileFuncParm)(const TCHAR * FileName));


//--------------------------------------------------------------------------
// Calculate some 64-bit file signature.  CRC and a checksum
//--------------------------------------------------------------------------
static void CalcCrc(Checksum_t * Check, char * Data, unsigned NumBytes)
{
    unsigned a;
    unsigned Reg, Sum;
    Reg = Check->Crc;
    Sum = Check->Sum;
    for(a=0;a<NumBytes;a++){
        Reg = Reg ^ Data[a];
        Sum = Sum + Data[a];
        Reg = (Reg >> 8) ^ ((Reg & 0xff) << 24) ^ ((Reg & 0xff) << 9);
        Sum = (Sum << 1) + (Sum >> 31);
    }
    Check->Crc = Reg;
    Check->Sum = Sum;
}

//--------------------------------------------------------------------------
// Clear line (erase the progress indicator)
//--------------------------------------------------------------------------
void ClearProgressInd(void)
{
    if (ProgressIndicatorVisible){
        _tprintf(_T("                                                                          \r"));
        ProgressIndicatorVisible = 0;
    }
}

//--------------------------------------------------------------------------
// Escape names for batch files: % turns into %%
//--------------------------------------------------------------------------
TCHAR * EscapeBatchName(TCHAR * Name)
{
    static TCHAR EscName[_MAX_PATH*2];
    int a,b;
    b = 0;
    for (a=0;;){
        EscName[b++] = Name[a];
        if (Name[a] == '\0') break;
        if (Name[a] == '%') EscName[b++] = '%'; // Escape '%' with '%%' for batch files.
        a++;
    }
    return EscName;
}

//--------------------------------------------------------------------------
// Eliminate duplicates.
//--------------------------------------------------------------------------
static int EliminateDuplicate(FileData_t ThisFile, FileData_t DupeOf)
{
    // First compare whole file.  If mismatch, return 0.
    #define CHUNK_SIZE 0x10000
    FILE * File1, * File2;
    unsigned BytesLeft;
    unsigned BytesToRead;
    TCHAR Buf1[CHUNK_SIZE], Buf2[CHUNK_SIZE];
    int IsDuplicate = 1;
    int Hardlinked = 0;
    int IsReadonly;
    struct _stat FileStat;

    if (ThisFile.FileSize != DupeOf.FileSize) return 0;

    Hardlinked = 0;
    if (DupeOf.NumLinks && memcmp(&ThisFile.FileIndex, &DupeOf.FileIndex, sizeof(ThisFile.FileIndex)) == 0){
        Hardlinked = 1;
        goto dont_read;
    }

    if (DupeOf.NumLinks >= 1023){
        // Do not link more than 1023 files onto one physical file (windows limit)
        return 0;
    }

    File1 = _tfopen(ThisFile.FileName, _T("rb"));
    if (File1 == NULL){
        return 0;
    }
    File2 = _tfopen(DupeOf.FileName, _T("rb"));
    if (File2 == NULL){
        fclose(File1);
        return 0;
    }

    BytesLeft = ThisFile.FileSize;

    while(BytesLeft){
        BytesToRead = BytesLeft;
        if (BytesToRead > CHUNK_SIZE) BytesToRead = CHUNK_SIZE;

        if (fread(Buf1, 1, BytesToRead, File1) != BytesToRead){
            ClearProgressInd();
            _ftprintf(stderr, _T("Error doing full file read on '%s'\n"), ThisFile.FileName);
        }

        if (fread(Buf2, 1, BytesToRead, File2) != BytesToRead){
            ClearProgressInd();
            _ftprintf(stderr, _T("Error doing full file read on '%s'\n"), DupeOf.FileName);
        }

        BytesLeft -= BytesToRead;

        if (memcmp(Buf1, Buf2, BytesToRead)){
            IsDuplicate = 0;
            break;
        }
    }

    fclose(File1);
    fclose(File2);

    if (!IsDuplicate){
        // Full file duplicate check failed (CRC collision, or differs only after 32k)
        return 0;
    }

    DupeStats.DuplicateFiles += 1;
    DupeStats.DuplicateBytes += (__int64)ThisFile.FileSize;

dont_read:
    if (PrintDuplicates){
        if (!HardlinkSearchMode){
            ClearProgressInd();
            _tprintf(_T("Duplicate: '%s'\n"),DupeOf.FileName);
            _tprintf(_T("With:      '%s'\n"),ThisFile.FileName);
            if (Hardlinked){
                // If the files happen to be hardlinked, show that.
                _tprintf(_T("    (hardlinked instances of same file)\n"));
            }
        }
    }


    if (_tstat(ThisFile.FileName, &FileStat) != 0){
        // oops!
        _ftprintf(stderr, _T("_tstat failed on '%s'\n"), ThisFile.FileName);
        exit (EXIT_FAILURE);
    }
    IsReadonly = (FileStat.st_mode & S_IWUSR) ? 0 : 1;

    if (IsReadonly){
        // Readonly file.
        if (!DoReadonly){
            ClearProgressInd();
            _tprintf(_T("Skipping duplicate readonly file '%s'\n"),
                        ThisFile.FileName);
            return 1;
        }
        if (MakeHardLinks || DelDuplicates){
            // Make file read/write so we can delete it.
            // We sort of assume we own the file.  Otherwise, not much we can do.
            _tchmod(ThisFile.FileName, FileStat.st_mode | S_IWUSR);
        }
    }

    if (BatchFile){
        // put command in batch file
        _ftprintf(BatchFile, _T("del %s \"%s\"\n"),IsReadonly ? _T("/F"):_T(""),
                EscapeBatchName(ThisFile.FileName));
        if (!DelDuplicates){
            _ftprintf(BatchFile, _T("fsutil hardlink create \"%s\" \"%s\"\n"),
                    ThisFile.FileName, DupeOf.FileName);
            if (IsReadonly){
                // If original was readonly, restore that attribute
                _ftprintf(BatchFile, _T("attrib +r \"%s\"\n"), ThisFile.FileName);
            }
        }else{
            _ftprintf(BatchFile, _T("rem duplicate of \"%s\"\n"), DupeOf.FileName);
        }

    }else if (MakeHardLinks || DelDuplicates){
        if (MakeHardLinks && Hardlinked) return 0; // Nothign to do.

        if (_tunlink(ThisFile.FileName)){
            ClearProgressInd();
            _ftprintf(stderr, _T("Delete of '%s' failed\n"), DupeOf.FileName);
            exit (EXIT_FAILURE);
        }
        if (MakeHardLinks){
            if (CreateHardLink(ThisFile.FileName, DupeOf.FileName, NULL) == 0){
                // Uh-oh.  Better stop before we mess up more stuff!
                ClearProgressInd();
                _ftprintf(stderr, _T("Create hard link from '%s' to '%s' failed\n"),
                        DupeOf.FileName, ThisFile.FileName);
                exit(EXIT_FAILURE);
            }

            {
                // set Unix access rights and time to new file
                struct _utimbuf mtime;
                _tchmod(ThisFile.FileName, FileStat.st_mode);

                // Set mod time to original file's
                mtime.actime = FileStat.st_mtime;
                mtime.modtime = FileStat.st_mtime;

                _tutime(ThisFile.FileName, &mtime);
            }
            ClearProgressInd();
            _tprintf(_T("    Created hardlink\n"));
        }else{
            ClearProgressInd();
            _tprintf(_T("    Deleted duplicate\n"));
        }
    }
    return 2;
}

//--------------------------------------------------------------------------
// Check for duplicates.
//--------------------------------------------------------------------------
static void CheckDuplicate(FileData_t ThisFile)
{
    int Ptr;
    int * Link;
    // Find where in the trie structure it belongs.
    Ptr = 0;

    DupeStats.TotalFiles += 1;
    DupeStats.TotalBytes += (__int64) ThisFile.FileSize;

    if (NumUnique == 0) goto store_it;

    for(;;){
        int comp;
        comp = memcmp(&ThisFile.Checksum, &FileData[Ptr].Checksum, sizeof(Checksum_t));
        if (comp == 0){
            // Check for true duplicate.
            if (!ReferenceFiles && !HardlinkSearchMode){
                int r = EliminateDuplicate(ThisFile, FileData[Ptr]);
                if (r){
                    if (r == 2) FileData[Ptr].NumLinks += 1; // Update link count.
                    // Its a duplicate for elimination.  Do not store info on it.
                    return;
                }
            }
            // Build a chain on one side of the branch.
            // That way, we will check every checksum collision from here on.
            comp = 1;
        }

        if (comp){
            if (comp > 0){
                Link = &FileData[Ptr].Larger;
            }else{
                Link = &FileData[Ptr].Smaller;
            }
            if (*Link < 0){
                // Link it to here.
                *Link = NumUnique;
                break;
            }else{
                Ptr = *Link;
            }
        }
    }

    store_it:

    if (NumUnique >= NumAllocated){
        // Array is full.  Make it bigger
        NumAllocated = NumAllocated + NumAllocated/2;
        FileData = realloc(FileData, sizeof(FileData_t) * NumAllocated);
        if (FileData == NULL){
            _ftprintf(stderr, _T("Malloc failure"));
            exit(EXIT_FAILURE);
        }
    }
    FileData[NumUnique] = ThisFile;
    NumUnique += 1;
}

//--------------------------------------------------------------------------
// Walk the file tree after handling detect mode to show linked groups.
//--------------------------------------------------------------------------
static void WalkTree(int index, int LinksFirst, int GroupLen)
{
    int a,t;

    if (NumUnique == 0) return;

    if (FileData[index].Larger >= 0){
        int Larger = FileData[index].Larger;
        if (memcmp(&FileData[Larger].Checksum, &FileData[index].Checksum, sizeof(Checksum_t)) == 0){
            // it continues the same group.
            WalkTree(FileData[index].Larger,LinksFirst >= 0 ? LinksFirst : index, GroupLen+1);
            goto not_end;
        }else{
            WalkTree(FileData[index].Larger,-1,0);
        }
    }
    _tprintf(_T("\nHardlink group, %d of %d hardlinked instances found in search tree:\n"), GroupLen+1, FileData[index].NumLinks);
    t = LinksFirst >= 0 ? LinksFirst : index;
    for (a=0;a<=GroupLen;a++){
        _tprintf(_T("  \"%s\"\n"),FileData[t].FileName);
        t = FileData[t].Larger;
    }

    DupeStats.HardlinkGroups += 1;

not_end:
    if (FileData[index].Smaller >= 0){
        WalkTree(FileData[index].Smaller,-1,0);
    }
}

//--------------------------------------------------------------------------
// Do selected operations to one file at a time.
//--------------------------------------------------------------------------
static void ProcessFile(const TCHAR * FileName)
{
    unsigned FileSize;
    Checksum_t CheckSum;
    struct _stat FileStat;
    int i;

    /* skip if contains substrings */
    for (i = 0; i < NumIgnPats; i++)
        if (_tcsstr(FileName, IgnPats[i]) )
        {
            DupeStats.IgnoredFiles++;
            return;
        }

    FileData_t ThisFile;
    memset(&ThisFile, 0, sizeof(ThisFile));

    {
        static int LastPrint, Now;
        Now = GetTickCount();
        if ((unsigned)(Now-LastPrint) > 200){
            if (ShowProgress){
                TCHAR ShowName[55];
                int l = _tcslen(FileName);
#ifdef _UNICODE
                wmemset(ShowName, ' ', 54);
#else
                memset(ShowName, ' ', 54);
#endif
                ShowName[54] = 0;
                if (l > 50) l = 51;
                memcpy(ShowName, FileName, l*sizeof(TCHAR));
                if (l >= 51) _tcscpy(ShowName+50,_T("..."));

                _tprintf(_T("Scanned %4d files: %s\r"),FilesMatched, ShowName);
                LastPrint = Now;
                ProgressIndicatorVisible = 1;
            }
            fflush(stdout);
        }
    }

    FilesMatched += 1;

    if (BatchFileName && _tcscmp(FileName, BatchFileName) == 0) return;

    if (_tstat(FileName, &FileStat) != 0){
        // oops!
        goto cant_read_file;
    }
    FileSize = FileStat.st_size;

    if (FileSize == 0){
        if (SkipZeroLength){
            DupeStats.ZeroLengthFiles += 1;
            return;
        }
    }

    ThisFile.Larger = -1;
    ThisFile.Smaller = -1;
    ThisFile.FileSize = FileSize;

    {
        HANDLE FileHandle;
        BY_HANDLE_FILE_INFORMATION FileInfo;
        FileHandle = CreateFile(FileName,
                        GENERIC_READ,         // dwDesiredAccess
                        FILE_SHARE_READ,      // dwShareMode
                        NULL,                 // Security attirbutes
                        OPEN_EXISTING,        // dwCreationDisposition
                        FILE_ATTRIBUTE_NORMAL,// dwFlagsAndAttributes.  Ignored for opening existing files
                        NULL);                // hTemplateFile.  Ignored for existing.
        if (FileHandle == (void *)-1){
cant_read_file:
            DupeStats.CantReadFiles += 1;
            if (!HideCantReadMessage){
                ClearProgressInd();
                _ftprintf(stderr,_T("Could not read '%s'\n"),FileName);
            }
            return;
        }

        GetFileInformationByHandle(FileHandle, &FileInfo);

        CloseHandle(FileHandle);

        if (Verbose){
            ClearProgressInd();
            _tprintf(_T("Hardlinked (%d links) node=%08x %08x: %s\n"),FileInfo.nNumberOfLinks,
                FileInfo.nFileIndexHigh, FileInfo.nFileIndexLow, FileName);
        }

        if (HardlinkSearchMode && FileInfo.nNumberOfLinks == 1){
            // File has only one link, so its not hardlinked.  Skip for hardlink search mode.
            return;
        }

        //_tprintf(_T("    Info:  Index: %08x %08x\n"),FileInfo.nFileIndexHigh, FileInfo.nFileIndexLow);

        // Use the file index (which is NTFS equivalent of the iNode) instead of the CRC.
        ThisFile.FileIndex.Low      = FileInfo.nFileIndexLow;
        ThisFile.FileIndex.High     = FileInfo.nFileIndexHigh;
        ThisFile.NumLinks = FileInfo.nNumberOfLinks;

        if (HardlinkSearchMode){
            // For hardlink search mode, duplicates are detected by file index, not CRC,
            // so copy the file ID into the CRC.
            ThisFile.Checksum.Sum = ThisFile.FileIndex.Low;
            ThisFile.Checksum.Crc = ThisFile.FileIndex.High;
        }
    }

    if (!HardlinkSearchMode){
        FILE * infile;
        char FileBuffer[BYTES_DO_CHECKSUM_OF];
        unsigned BytesRead, BytesToRead;
        memset(&CheckSum, 0, sizeof(CheckSum));

        infile = _tfopen(FileName, _T("rb"));

        if (infile == NULL) {
            if (!HideCantReadMessage){
                ClearProgressInd();
                _ftprintf(stderr, _T("can't open '%s'\n"), FileName);
            }
            return;
        }

        BytesToRead = FileSize;
        if (BytesToRead > BYTES_DO_CHECKSUM_OF) BytesToRead = BYTES_DO_CHECKSUM_OF;
        BytesRead = fread(FileBuffer, 1, BytesToRead, infile);
        if (BytesRead != BytesToRead){
            if (!HideCantReadMessage){
                ClearProgressInd();
                _ftprintf(stderr,_T("file read problem on '%s'\n"), FileName);
            }
            return;
        }

        CalcCrc(&CheckSum, FileBuffer, BytesRead);
        fclose(infile);

        CheckSum.Sum += FileSize;
        if (PrintFileSigs){
            ClearProgressInd();
            _tprintf(_T("%08x%08x %10d %s\n"),CheckSum.Crc, CheckSum.Sum, FileSize, FileName);
        }

        ThisFile.Checksum = CheckSum;
        ThisFile.FileSize = FileSize;
    }

    ThisFile.FileName = _tcsdup(FileName); // allocate the string last, so
                                          // we don't waste memory on errors.
    CheckDuplicate(ThisFile);
}

//--------------------------------------------------------------------------
// complain about bad state of the command line.
//--------------------------------------------------------------------------
static void Usage (void)
{
    _tprintf(_T("finddupe v"VERSION" compiled "__DATE__"\n"));
    _tprintf(_T("Usage: finddupe [options] [-ref] <filepat> [filepat]...\n"));
    _tprintf(_T("Options:\n"
           " -bat <file.bat> Create batch file with commands to do the hard\n"
           "                 linking.  run batch file afterwards to do it\n"
           " -hardlink       Create hardlinks.  Works on NTFS file systems only.\n"
           "                 Use with caution!\n"
           " -del            Delete duplicate files\n"
           " -v              Verbose\n"
           " -sigs           Show signatures calculated based on first 32k for each file\n"
           " -rdonly         Apply to readonly files also (as opposed to skipping them)\n"
           " -ref <filepat>  Following file pattern are files that are for reference, NOT\n"
           "                 to be eliminated, only used to check duplicates against\n"
           " -z              Do not skip zero length files (zero length files are ignored\n"
           "                 by default)\n"
           " -u              Do not print a warning for files that cannot be read\n"
           " -p              Hide progress indicator (useful when redirecting to a file)\n"
           " -j              Follow NTFS junctions and reparse points (off by default)\n"
           " -listlink       hardlink list mode.  Not valid with -del, -bat, -hardlink,\n"
           "                 or -rdonly, options\n"
           " -ign <substr>   Ignore file pattern, like .git, .svn or .bak (can be repeated)\n"
           " -depth <num>    Maximum recursion depth, default 0 = infinite\n"
           "  <filepat>      Pattern for files.  Examples:\n"
           "                  c:\\**        Match everything on drive C\n"
           "                  c:\\**\\*.jpg  Match only .jpg files on drive C\n"
           "                  **\\foo\\**    Match any path with component foo\n"
           "                               from current directory down\n"

           ));
    exit(EXIT_FAILURE);
}

//--------------------------------------------------------------------------
// The main program.
//--------------------------------------------------------------------------
int _tmain (int argc, TCHAR **argv)
{
    int argn;
    TCHAR * arg;
    TCHAR DefaultDrive;
    TCHAR DriveUsed = '\0';

    PrintDuplicates = 1;
    PrintFileSigs = 0;
    HardlinkSearchMode = 0;
    Verbose = 0;

    for (argn=1;argn<argc;argn++){
        arg = argv[argn];
        if (arg[0] != '-') break; // Filenames from here on.

        if (!_tcscmp(arg,_T("-h"))){
            Usage();
            exit(EXIT_FAILURE);

        }else if (!_tcscmp(arg,_T("-bat"))){
            BatchFileName = argv[++argn];
        }else if (!_tcscmp(arg,_T("-v"))){
            PrintDuplicates = 1;
            PrintFileSigs = 1;
            Verbose = 1;
            HideCantReadMessage = 0;
        }else if (!_tcscmp(arg,_T("-sigs"))){
            PrintDuplicates = 0;
            PrintFileSigs = 1;
        }else if (!_tcscmp(arg,_T("-hardlink"))){
            MakeHardLinks = 1;
        }else if (!_tcscmp(arg,_T("-del"))){
            DelDuplicates = 1;
        }else if (!_tcscmp(arg,_T("-rdonly"))){
            DoReadonly = 1;
        }else if (!_tcscmp(arg,_T("-listlink"))){
            HardlinkSearchMode = 1;
        }else if (!_tcscmp(arg,_T("-ref"))){
            break;
        }else if (!_tcscmp(arg,_T("-z"))){
            SkipZeroLength = 0;
        }else if (!_tcscmp(arg,_T("-u"))){
            HideCantReadMessage = 1;
        }else if (!_tcscmp(arg,_T("-p"))){
            ShowProgress = 0;
        }else if (!_tcscmp(arg,_T("-j"))){
            FollowReparse = 1;
        }else if (!_tcscmp(arg,_T("-ign"))){
            if (NumIgnPats < NELEMS(IgnPats) - 2)
              IgnPats[NumIgnPats++] = argv[++argn];
            else
            {
              _tprintf(_T("Too many -ign patterns\n"));
              exit(EXIT_FAILURE);
            }
        }else if (!_tcscmp(arg,_T("-depth"))){
            MaxDepth = _tstoi(argv[++argn]);
        }else{
            _tprintf(_T("Argument '%s' not understood.  Use -h for help.\n"),arg);
            exit(-1);
        }
    }

    if (argn > argc){
        _ftprintf(stderr, _T("Missing argument!  Use -h for help\n"));
        exit(EXIT_FAILURE);
    }

    if (argn == argc){
        _ftprintf(stderr, _T("No files to process.   Use -h for help\n"));
        exit(EXIT_FAILURE);
    }

    if (HardlinkSearchMode){
        if (BatchFileName || MakeHardLinks || DelDuplicates || DoReadonly || BatchFileName){
            _ftprintf(stderr, _T("listlink option is not valid with any other"
                " options other than -v\n"));
            exit(EXIT_FAILURE);
        }
    }

    NumUnique = 0;
    NumAllocated = 1024;
    FileData = malloc(sizeof(FileData_t)*1024);
    if (FileData == NULL){
        _ftprintf(stderr, _T("Malloc failure"));
        exit(EXIT_FAILURE);
    }

    if (BatchFileName){
        BatchFile = _tfopen(BatchFileName, _T("w"));
        if (BatchFile == NULL){
            _tprintf(_T("Unable to open task batch file '%s'\n"),BatchFileName);
        }
        _ftprintf(BatchFile, _T("@echo off\n"));
        _ftprintf(BatchFile, _T("REM Batch file for replacing duplicates with hard links\n"));
        _ftprintf(BatchFile, _T("REM created by finddupe program\n\n"));
    }

    memset(&DupeStats, 0, sizeof(DupeStats));

    {
        TCHAR CurrentDir[_MAX_PATH];
        _tgetcwd(CurrentDir, sizeof(CurrentDir));
        DefaultDrive = _totlower(CurrentDir[0]);
    }


    for (;argn<argc;argn++){
        int a;
        TCHAR Drive;
        FilesMatched = 0;

        if (!_tcscmp(argv[argn],_T("-ref"))){
            ReferenceFiles = 1;
            argn += 1;
            if (argn >= argc) continue;
        }else{
            ReferenceFiles = 0;
        }

        for (a=0;;a++){
            if (argv[argn][a] == '\0') break;
            if (argv[argn][a] == '/') argv[argn][a] = '\\';
        }

        if (argv[argn][1] == ':'){
            Drive = _totlower(argv[argn][0]);
        }else{
            Drive = DefaultDrive;
        }
        if (DriveUsed == '\0') DriveUsed = Drive;
        if (DriveUsed != Drive){
            if (MakeHardLinks){
                _ftprintf(stderr, _T("Error: Hardlinking across different drives not possible\n"));
                return EXIT_FAILURE;
            }
        }

        // Use my globbing module to do fancier wildcard expansion with recursive
        // subdirectories under Windows.
        MyGlob(argv[argn], FollowReparse, ProcessFile);

        if (!FilesMatched){
            _ftprintf(stderr, _T("Error: No files matched '%s'\n"), argv[argn]);
        }
    }

    if (HardlinkSearchMode){
        ClearProgressInd();
        _tprintf(_T("\n"));
        DupeStats.HardlinkGroups = 0;
        WalkTree(0,-1,0);
        _tprintf(_T("\nNumber of hardlink groups found: %d\n"),DupeStats.HardlinkGroups);
    }else{
        if (DupeStats.TotalFiles == 0){
            _ftprintf(stderr, _T("No files to process\n"));
            return EXIT_FAILURE;
        }

        if (BatchFile){
            fclose(BatchFile);
            BatchFile = NULL;
        }

        // Print summary data
        ClearProgressInd();
        _tprintf(_T("\n"));
        _tprintf(_T("Files: %8u kBytes in %5d files\n"),
                (unsigned)(DupeStats.TotalBytes/1000), DupeStats.TotalFiles);
        _tprintf(_T("Dupes: %8u kBytes in %5d files\n"),
                (unsigned)(DupeStats.DuplicateBytes/1000), DupeStats.DuplicateFiles);
    }
    if (DupeStats.ZeroLengthFiles){
        _tprintf(_T("  %d files of zero length were skipped\n"),DupeStats.ZeroLengthFiles);
    }
    if (DupeStats.IgnoredFiles){
        _tprintf(_T("  %d files were ignored\n"),DupeStats.IgnoredFiles);
    }
    if (DupeStats.CantReadFiles){
        _tprintf(_T("  %d files could not be opened\n"),DupeStats.CantReadFiles);
    }

    return EXIT_SUCCESS;
}
