# finddupe
Unicode / MinGW port of [finddupe](http://www.sentex.net/~mwandel/finddupe/) duplicate file detector and eliminator for Windows by Matthias Wandel.

## Reasons
I really like finddupe when I look for duplicate files. It is fast and clever thanks to CRC file signatures. It can also find NTFS hard links, which is  difficult otherwise. Please refer to [Matthias site](http://www.sentex.net/~mwandel/finddupe/) for full description. My favourites are
`finddupe c:\MyBooks` and `finddupe -listlink c:\MyBooks`.

However, his current version 1.23 is ASCII-only and fails on non-ASCII filenames, as is often the case with books.

## Port
I ported finddupe to Unicode (actually UTF-16) and MinGW as version 1.24. I used `tchar.h` wrapper, so ASCII build is still possible. It works for me, but some more testing is desirable. 

I hate this wchar_t stuff, but I really like finddupe. Why didn't Microsoft go for UTF-8? [utf8everywhere.org](http://utf8everywhere.org/) makes an interesting reading.

MinGW-w64 is required for Unicode wmain as described [here](https://sourceforge.net/p/mingw-w64/wiki2/Unicode%20apps/). I used Ruby DevKit-mingw64-32-4.7.2 from [rubyinstaller.org](http://rubyinstaller.org/downloads/).   
Didn't test 64 bit build yet.

I provide [binary](finddupe.exe), so you do not have to compile yourself.

## License
Original license by Matthias Wandel: Finddupe is totally free. Do whatever you like with it. You can integrate it into GPL or BSD style licensed programs if you would like to.
