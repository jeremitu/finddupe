//--------------------------------------------------------------------------------
// Module to do recursive directory file matching under windows.
//
// Tries to do pattern matching to produce similar results as Unix, but using
// the Windows _findfirst to do all the pattern matching.
//
// Also hadles recursive directories - "**" path component expands into
// any levels of subdirectores (ie c:\**\*.c matches ALL .c files on drive c:)
//
// Matthias Wandel Nov 5 2000 - March 2009
//--------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <io.h>
#include <sys/stat.h>
#define WIN32_LEAN_AND_MEAN // To keep windows.h bloat down.
#include <windows.h>
#include <tchar.h>

#define TRUE 1
#define FALSE 0

//#define DEBUGGING

typedef struct {
    TCHAR * Name;
    int attrib;
}FileEntry;

#ifdef DEBUGGING
//--------------------------------------------------------------------------------
// Dummy function to show operation.
//--------------------------------------------------------------------------------
void ShowName(const TCHAR * FileName)
{
    _tprintf(_T("     %s\n"),FileName);
}
#endif

//--------------------------------------------------------------------------------
// Simple path splicing (assumes no '\' in either part)
//--------------------------------------------------------------------------------
static int CatPath(TCHAR * dest, const TCHAR * p1, const TCHAR * p2)
{
    int l;
    l = _tcslen(p1);
    if (!l){
        _tcscpy(dest, p2);
    }else{
        if (l+_tcslen(p2) > _MAX_PATH-2){
            //_ftprintf(stderr, _T("\n\n\nPath too long:    \n    %s + %s\n"),p1,p2);
            return 0;
        }
        memcpy(dest, p1, (l+1)*sizeof(TCHAR));
        if (dest[l-1] != '\\' && dest[l-1] != ':'){
            dest[l++] = '\\';
        }
        _tcscpy(dest+l, p2);
    }
    return 1;
}

//--------------------------------------------------------------------------------
// Qsort compare function
//--------------------------------------------------------------------------------
int CompareFunc(const void * f1, const void * f2)
{
    return _tcscmp(((FileEntry *)f1)->Name,((FileEntry *)f2)->Name);
}


//--------------------------------------------------------------------------------
// Check if directory is a reparse point
//--------------------------------------------------------------------------------
int IsReparsePoint(TCHAR * DirName)
{
    HANDLE FileHandle;
    BY_HANDLE_FILE_INFORMATION FileInfo;

    FileHandle = CreateFile(DirName,
                    0,                    // dwDesiredAccess
                    FILE_SHARE_READ,      // dwShareMode
                    NULL,                 // Security attirbutes
                    OPEN_EXISTING,        // dwCreationDisposition
                    FILE_FLAG_BACKUP_SEMANTICS | // dwFlagsAndAttributes.  Need this to do dirs.
                    FILE_FLAG_OPEN_REPARSE_POINT, // Need this flag to open the reparse point instead of following it.
                    NULL);                // hTemplateFile.  Ignored for existing.
    if (FileHandle == (void *)-1){
        return FALSE;
    }

    if (!GetFileInformationByHandle(FileHandle, &FileInfo)){
        return FALSE;
    }

    // Directory node is in: FileInfo.nFileIndexHigh, FileInfo.nFileIndexLow

    if (FileInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT){
        return TRUE;
    }else{
        return FALSE;
    }
}

//--------------------------------------------------------------------------------
// Decide how a particular pattern should be handled, and call function for each.
//--------------------------------------------------------------------------------
static void Recurse(const TCHAR * Pattern, int FollowReparse, void (*FileFuncParm)(const TCHAR * FileName))
{
    TCHAR BasePattern[_MAX_PATH];
    TCHAR MatchPattern[_MAX_PATH];
    TCHAR PatCopy[_MAX_PATH*2];

    int a;
    int MatchDirs;
    int BaseEnd, PatternEnd;
    int SawPat;
    int StarStarAt;

    _tcscpy(PatCopy, Pattern);

    #ifdef DEBUGGING
        _tprintf(_T("\nCalled with '%s'\n"),Pattern);
    #endif

DoExtraLevel:
    MatchDirs = TRUE;
    BaseEnd = 0;
    PatternEnd = 0;

    SawPat = FALSE;
    StarStarAt = -1;

    // Split the path into base path and pattern to match against using findfirst.
    for (a=0;;a++){
        if (PatCopy[a] == '*' || PatCopy[a] == '?'){
            SawPat = TRUE;
        }

        if (PatCopy[a] == '*' && PatCopy[a+1] == '*'){
            if (a == 0 || PatCopy[a-1] == '\\' || PatCopy[a-1] == ':'){
                if (PatCopy[a+2] == '\\' || PatCopy[a+2] == '\0'){
                    // x\**\y  ---> x\y  x\*\**\y
                    StarStarAt = a;
                    if (PatCopy[a+2]){
                        memcpy(PatCopy+a, PatCopy+a+3, (_tcslen(PatCopy)-a-1)*sizeof(TCHAR));
                    }else{
                        PatCopy[a+1] = '\0';
                    }
                }
            }
        }

        if (PatCopy[a] == '\\' || (PatCopy[a] == ':' && PatCopy[a+1] != '\\')){
            PatternEnd = a;
            if (SawPat) break; // Findfirst can only match one level of wildcard at a time.
            BaseEnd = a+1;
        }
        if (PatCopy[a] == '\0'){
            PatternEnd = a;
            MatchDirs = FALSE;
            break;
        }
    }

    _tcsncpy(BasePattern, PatCopy, BaseEnd);
    BasePattern[BaseEnd] = 0;

    _tcsncpy(MatchPattern, PatCopy, PatternEnd);
    MatchPattern[PatternEnd] = 0;

    #ifdef DEBUGGING
        _tprintf(_T("Base:%s  Pattern:%s dirs:%d\n"),BasePattern, MatchPattern, MatchDirs);
    #endif

    {
        FileEntry * FileList = NULL;
        int NumAllocated = 0;
        int NumHave = 0;

        struct _tfinddata_t finddata;
        long find_handle;

        find_handle = _tfindfirst(MatchPattern, &finddata);

        for (;;){
            if (find_handle == -1) break;

            // Eliminate the obvious patterns.
            if (!_tcscmp(finddata.name, _T("."))) goto next_file;
            if (!_tcscmp(finddata.name, _T(".."))) goto next_file;

            if (finddata.attrib & _A_SUBDIR){
                if (!MatchDirs) goto next_file;
            }else{
                if (MatchDirs) goto next_file;
            }

            // Add it to the list.
            if (NumAllocated <= NumHave){
                NumAllocated = NumAllocated+10+NumAllocated/2;
                FileList = realloc(FileList, NumAllocated * sizeof(FileEntry));
                if (FileList == NULL) goto nomem;
            }
            a = _tcslen(finddata.name);
            FileList[NumHave].Name = malloc((a+1)*sizeof(TCHAR));
            if (FileList[NumHave].Name == NULL){
                nomem:
                _tprintf(_T("malloc failure\n"));
                exit(-1);
            }
            memcpy(FileList[NumHave].Name, finddata.name, (a+1)*sizeof(TCHAR));
            FileList[NumHave].attrib = finddata.attrib;
            NumHave++;

            next_file:
            if (_tfindnext(find_handle, &finddata) != 0) break;
        }
        _findclose(find_handle);

        // Sort the list...
        qsort(FileList, NumHave, sizeof(FileEntry), CompareFunc);

        // Use the list.
        for (a=0;a<NumHave;a++){
            TCHAR CombinedName[_MAX_PATH*2];
            if (FileList[a].attrib & _A_SUBDIR){
                if (CatPath(CombinedName, BasePattern, FileList[a].Name)){
                    if (FollowReparse || !IsReparsePoint(CombinedName)){
                        _tcscat(CombinedName, PatCopy+PatternEnd);
                        Recurse(CombinedName, FollowReparse, FileFuncParm);
                    }
                }
            }else{
                if (CatPath(CombinedName, BasePattern, FileList[a].Name)){
                    FileFuncParm(CombinedName);
                }
            }
            free(FileList[a].Name);
        }
        free(FileList);
    }

    if(StarStarAt >= 0){
        _tcscpy(MatchPattern, PatCopy+StarStarAt);
        PatCopy[StarStarAt] = 0;
        _tcscpy(PatCopy+StarStarAt, _T("*\\**\\"));
        _tcscat(PatCopy, MatchPattern);

        #ifdef DEBUGGING
            _tprintf(_T("Recurse with '%s'\n"),PatCopy);
        #endif

        // As this function context is no longer needed, we can just goto back
        // to the top of it to avoid adding another context on the stack.
        goto DoExtraLevel;
    }
}

//--------------------------------------------------------------------------------
// Do quick precheck - if no wildcards, and it names a directory, do whole dir.
//--------------------------------------------------------------------------------
int MyGlob(const TCHAR * Pattern, int FollowReparse, void (*FileFuncParm)(const TCHAR * FileName))
{
    int a;
    TCHAR PathCopy[_MAX_PATH];

    _tcsncpy(PathCopy, Pattern, _MAX_PATH-1);
    a = _tcslen(PathCopy);
    if (a && PathCopy[a-1] == '\\'){ // Ends with backslash
        if (!(a == 3 && PathCopy[1] == ':')){
            // and its not something like c:\, then delete the trailing backslash
            PathCopy[a-1] = '\0';
        }
    }

    for (a=0;;a++){
        if (PathCopy[a] == '*' || PathCopy[a] == '?') break; // Contains wildcards
        if (PathCopy[a] == '\0') break;
    }

    if (PathCopy[a] == '\0'){
        // No wildcards were specified.  Do a whole tree, or file.
        struct _stat FileStat;
        if (_tstat(PathCopy, &FileStat) != 0){
            // There is no file or directory by that name.
            return -1;
            _tprintf(_T("Stat failed\n"));
        }
        if (FileStat.st_mode & 040000){
            if (CatPath(PathCopy, PathCopy, _T("**"))){
                Recurse(PathCopy, FollowReparse, FileFuncParm);
            }
        }else{
            FileFuncParm(PathCopy);
        }
    }else{
        // A wildcard was specified.
        Recurse(PathCopy, FollowReparse, FileFuncParm);
    }
    return 0;
}




#ifdef DEBUGGING
//--------------------------------------------------------------------------------
// The main program.
//--------------------------------------------------------------------------------
int _tmain (int argc, TCHAR **argv)
{
    int argn;
    TCHAR * arg;

    for (argn=1;argn<argc;argn++){
        MyGlob(argv[argn], 1, ShowName);
    }
    return EXIT_SUCCESS;
}
#endif


/*

non-recursive test cases:

    e:\make*\*
    \make*\*
    e:*\*.c
    \*\*.c
    \*
    c:*.c
    c:\*
    ..\*.c


recursive test cases:
    **
    **\*.c
    c:\**\*.c
    c:**\*.c
    .\**
    ..\**
    c:\

*/
