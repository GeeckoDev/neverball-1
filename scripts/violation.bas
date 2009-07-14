/'
 ' This simple FB program checks source files for length and vertical
 ' whitespace related violations.
 '
 ' The file.bi is a compiler file.
 '/
#include "file.bi"
dim as ushort MaxLen, MaxVertWhitespace
dim as uinteger VioCount(2,2), TotalSize, FileCount
dim as string File
const FileOut = "stdout.txt"
const Resources = "violationrc"

/'
 ' The violationrc format is this.
 ' * First line is maximum length.
 ' * Second line is the maximum number of consecutive vertical lines. A blank
 ' line will break this combo.
 ' * Subsequent lines are the filesnames and (if applicable) their relative
 ' paths. For platform independency, use forward slashes.
 '
 ' Will spit out information to stdout.txt
 '/
open FileOut for output as #1
open Resources for input as #2
input #2, MaxLen
input #2, MaxVertWhitespace
input #2, ""

if eof(2) = 0 then
    do
        VioCount(1,1) = 0
        VioCount(2,1) = 0
        line input #2, File
        if FileExists(File) then
            dim as string PresentLine
            dim as uinteger CurLine, LineCombo
            print "Opening file ";File;"... ";
            print #1, "File ";File;" ("& FileLen(File);" bytes) opened."
            TotalSize += FileLen(File)
            FileCount += 1
            open File for input as #3
            do
                CurLine += 1
                line input #3, PresentLine
                if PresentLine = "" then
                    if LineCombo > MaxVertWhitespace then
                        VioCount(2,1) += 1
                        VioCount(2,2) += 1
                        print #1, "* Vertical whitespace violation found ";_
                            "within Lines "& CurLine-LineCombo;"-"&_
                            CurLine-1;"."
                        print #1, "Consider adding a break somewhere within";
                        print #1, " this section."
                    end if
                    LineCombo = 0

                else
                    LineCombo += 1
                    if len(PresentLine) > MaxLen then
                        VioCount(1,1) += 1
                        VioCount(1,2) += 1
                        print #1, "* Length violation found at Line ";CurLine;".";
                        print #1, " There are "& len(PresentLine)-MaxLen;
                        print #1, " character(s) too many."
                        print #1, string(MaxLen,"-");"|"
                        print #1, PresentLine
                        print #1, string(MaxLen,"-");"|"
                    end if
                end if
            loop until eof(3)
            close #3
            print "closed."
            print #1, "File ";File;" closed. ";
            if VioCount(1,1) > 0 OR VioCount(2,1) > 0 then
                print #1, VioCount(1,1);" length and ";VioCount(2,1);_
                    " vertical whitespace violation(s) found."
                print #1, "Per kilobyte (SI), there are ";
                print #1, using "##.##";VioCount(1,1)/FileLen(File)*1000;
                print #1, " length violations and ";
                print #1, using "##.##";VioCount(2,1)/FileLen(File)*1000;
                print #1, " vertical whitespace violations."
            else
                print #1, "No length or vertical whitespace violations found."
            end if
            print #1, ""
        else
            print #1, "File ";File;" is non-existant."
        end if
    loop until eof(2)
end if

close #2
print #1, "Checking complete."
print #1, "-- Statistics --"
print #1, "Total files: ";FileCount;" files"
print #1, "Total project size: ";TotalSize;" bytes"
print #1, "Average file size: ";
print #1, using "#######.### bytes/file";TotalSize/FileCount
print #1, "Total Length violations: ";VioCount(1,2)
print #1, "Length violations per kilobyte (SI): ";
print #1, using "##.###";VioCount(1,2)/TotalSize*1000
print #1, "Length violations per file: ";
print #1, using "##.###";VioCount(1,2)/FileCount
print #1, "Total Vertical whitespace violations: ";VioCount(2,2)
print #1, "Vertical whitespace violations per kilobyte (SI): ";
print #1, using "##.###";VioCount(2,2)/TotalSize*1000
print #1, "Vertical whitespace violations per file: ";
print #1, using "##.###";VioCount(2,2)/FileCount
close #1