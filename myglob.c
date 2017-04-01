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

#define TRUE 1
#define FALSE 0

//#define DEBUGGING

typedef struct {
    char * Name;
    int attrib;
}FileEntry;

#ifdef DEBUGGING
//--------------------------------------------------------------------------------
// Dummy function to show operation.
//--------------------------------------------------------------------------------
void ShowName(const char * FileName)
{
    printf("     %s\n",FileName);
}
#endif

//--------------------------------------------------------------------------------
// Simple path splicing (assumes no '\' in either part)
//--------------------------------------------------------------------------------
static int CatPath(char * dest, const char * p1, const char * p2)
{
    int l;
    l = strlen(p1);
    if (!l){
        strcpy(dest, p2);
    }else{
        if (l+strlen(p2) > _MAX_PATH-2){
            //fprintf(stderr,"\n\n\nPath too long:    \n    %s + %s\n",p1,p2);
            return 0;
        }
        memcpy(dest, p1, l+1);
        if (dest[l-1] != '\\' && dest[l-1] != ':'){
            dest[l++] = '\\';
        }
        strcpy(dest+l, p2);
    }
    return 1;
}

//--------------------------------------------------------------------------------
// Qsort compare function
//--------------------------------------------------------------------------------
int CompareFunc(const void * f1, const void * f2)
{
    return strcmp(((FileEntry *)f1)->Name,((FileEntry *)f2)->Name);
}


//--------------------------------------------------------------------------------
// Check if directory is a reparse point
//--------------------------------------------------------------------------------
int IsReparsePoint(char * DirName)
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
static void Recurse(const char * Pattern, int FollowReparse, void (*FileFuncParm)(const char * FileName))
{
    char BasePattern[_MAX_PATH];
    char MatchPattern[_MAX_PATH];
    char PatCopy[_MAX_PATH*2];

    int a;
    int MatchDirs;
    int BaseEnd, PatternEnd;
    int SawPat;
    int StarStarAt;

    strcpy(PatCopy, Pattern);

    #ifdef DEBUGGING
        printf("\nCalled with '%s'\n",Pattern);
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
                        memcpy(PatCopy+a, PatCopy+a+3, strlen(PatCopy)-a-1);
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

    strncpy(BasePattern, PatCopy, BaseEnd);
    BasePattern[BaseEnd] = 0;

    strncpy(MatchPattern, PatCopy, PatternEnd);
    MatchPattern[PatternEnd] = 0;

    #ifdef DEBUGGING
        printf("Base:%s  Pattern:%s dirs:%d\n",BasePattern, MatchPattern, MatchDirs);
    #endif

    {
        FileEntry * FileList = NULL;
        int NumAllocated = 0;
        int NumHave = 0;
        
        struct _finddata_t finddata;
        long find_handle;

        find_handle = _findfirst(MatchPattern, &finddata);

        for (;;){
            if (find_handle == -1) break;

            // Eliminate the obvious patterns.
            if (!memcmp(finddata.name, ".",2)) goto next_file;
            if (!memcmp(finddata.name, "..",3)) goto next_file;

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
            a = strlen(finddata.name);
            FileList[NumHave].Name = malloc(a+1);
            if (FileList[NumHave].Name == NULL){
                nomem:
                printf("malloc failure\n");
                exit(-1);
            }
            memcpy(FileList[NumHave].Name, finddata.name, a+1);
            FileList[NumHave].attrib = finddata.attrib;
            NumHave++;

            next_file:
            if (_findnext(find_handle, &finddata) != 0) break;
        }
        _findclose(find_handle);

        // Sort the list...
        qsort(FileList, NumHave, sizeof(FileEntry), CompareFunc);

        // Use the list.
        for (a=0;a<NumHave;a++){
            char CombinedName[_MAX_PATH*2];
            if (FileList[a].attrib & _A_SUBDIR){
                if (CatPath(CombinedName, BasePattern, FileList[a].Name)){
                    if (FollowReparse || !IsReparsePoint(CombinedName)){
                        strcat(CombinedName, PatCopy+PatternEnd);
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
        strcpy(MatchPattern, PatCopy+StarStarAt);
        PatCopy[StarStarAt] = 0;
        strcpy(PatCopy+StarStarAt, "*\\**\\");
        strcat(PatCopy, MatchPattern);
       
        #ifdef DEBUGGING
            printf("Recurse with '%s'\n",PatCopy);
        #endif

        // As this function context is no longer needed, we can just goto back
        // to the top of it to avoid adding another context on the stack.
        goto DoExtraLevel;
    }
}

//--------------------------------------------------------------------------------
// Do quick precheck - if no wildcards, and it names a directory, do whole dir.
//--------------------------------------------------------------------------------
int MyGlob(const char * Pattern, int FollowReparse, void (*FileFuncParm)(const char * FileName))
{
    int a;
    char PathCopy[_MAX_PATH];

    strncpy(PathCopy, Pattern, _MAX_PATH-1);
    a = strlen(PathCopy);
    if (a && PathCopy[a-1] == '\\'){ // Endsi with backslash
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
        struct stat FileStat;
        if (stat(PathCopy, &FileStat) != 0){
            // There is no file or directory by that name.
            return -1;
            printf("Stat failed\n");
        }
        if (FileStat.st_mode & 040000){
            if (CatPath(PathCopy, PathCopy, "**")){
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
int main (int argc, char **argv)
{
    int argn;
    char * arg;

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
