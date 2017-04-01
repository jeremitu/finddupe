# finddupe
Unicode / MinGW port of [finddupe](http://www.sentex.net/~mwandel/finddupe/) duplicate file detector and eliminator by Matthias Wandel.

# Reasons
I really like finddupe when I look for duplicate files. It is fast and clever thanks to CRC file signatures. It can also find NTFS hard links, which is very difficult otherwise. Please refer to [Matthias site](http://www.sentex.net/~mwandel/finddupe/) for full description. My favourites are
`finddupe c:\MyBooks` and `finddupe -listlink c:\MyBooks`.

However, his current version 1.23 is ASCII-only and fails on non-ASCII filenames, as is often the case with books.

#Port
I ported finddupe to Unicode and MinGW using tchar.h wrapper, so ASCII build is still possible. It works for me, but some more testing is desirable. 

MinGW-w64 is required for Unicode as described [here](https://sourceforge.net/p/mingw-w64/wiki2/Unicode%20apps/). I used Ruby DevKit from [rubyinstaller.org](http://rubyinstaller.org/downloads/).

I provide [binary](finddupe.exe) so you do not have to compile.

#License
Original license by Matthias Wandel: Finddupe is totally free. Do whatever you like with it. You can integrate it into GPL or BSD style licensed programs if you would like to.

