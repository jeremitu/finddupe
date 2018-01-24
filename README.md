# finddupe
Unicode / MinGW port of [finddupe](http://www.sentex.net/~mwandel/finddupe/) duplicate file detector and eliminator for Windows by Matthias Wandel.

## Reasons
I really like finddupe when I look for duplicate files. It is fast and clever. The match candidates are clustered according to the signature of the first 32k, then checked byte for byte. It can also create and find NTFS hard links. Creating hard links saves you disk space. Listing all existing hard links is very difficult otherwise.

Please refer to [Matthias site](http://www.sentex.net/~mwandel/finddupe/) for full description. My favourites are
`finddupe -hardlink c:\MyBooks` to remove duplicates in a big e-book collection and `finddupe -listlink c:\MyBooks` to list them.

However, Matthias' current version 1.23 is ASCII-only and fails on non-ASCII filenames, as is often the case with books.

## Port
I ported finddupe to Unicode (actually UTF-16) and MinGW as version 1.24. I used `tchar.h` wrapper, so ASCII build is still possible. It works for me, but some more testing is desirable. 

I hate this wchar_t stuff, but I really like finddupe. Why didn't Microsoft go for UTF-8? [utf8everywhere.org](http://utf8everywhere.org/) makes an interesting reading.

MinGW-w64 is required for Unicode wmain as described [here](https://sourceforge.net/p/mingw-w64/wiki2/Unicode%20apps/). I used Ruby DevKit-mingw64-32-4.7.2 and DevKit-mingw64-64-4.7-2 from [rubyinstaller.org](http://rubyinstaller.org/downloads/) for 64-bit build.

In version 1.25 I added -ign and -depth arguments to ignore files from the comparizon.

I provide [binary for Win32](finddupe.exe) and [binary for Win64](finddupe64.exe), so you do not have to compile yourself.

## Usage
```
finddupe v1.25 compiled May 11 2017
Usage: finddupe [options] [-ref] <filepat> [filepat]...
Options:
 -bat <file.bat> Create batch file with commands to do the hard
                 linking.  run batch file afterwards to do it
 -hardlink       Create hardlinks.  Works on NTFS file systems only.
                 Use with caution!
 -del            Delete duplicate files
 -v              Verbose
 -sigs           Show signatures calculated based on first 32k for each file
 -rdonly         Apply to readonly files also (as opposed to skipping them)
 -ref <filepat>  Following file pattern are files that are for reference, NOT
                 to be eliminated, only used to check duplicates against
 -z              Do not skip zero length files (zero length files are ignored
                 by default)
 -u              Do not print a warning for files that cannot be read
 -p              Hide progress indicator (useful when redirecting to a file)
 -j              Follow NTFS junctions and reparse points (off by default)
 -listlink       hardlink list mode.  Not valid with -del, -bat, -hardlink,
                 or -rdonly, options
 -ign <substr>   Ignore file pattern, like .git, .svn or .bak (can be repeated)
 -depth <num>    Maximum recursion depth, default 0 = infinite
 <filepat>       Pattern for files.  Examples:
                  c:\**        Match everything on drive C
                  c:\**\*.jpg  Match only .jpg files on drive C
                  **\foo\**    Match any path with component foo
                               from current directory down
```

I provide some real-life usage examples on [my homepage](https://www.jurasz.de/finddupe).

## License
Original license by Matthias Wandel: Finddupe is totally free. Do whatever you like with it. You can integrate it into GPL or BSD style licensed programs if you would like to.
