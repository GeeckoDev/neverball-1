dim shared as string MasterDir
MasterDir = curdir
const Strings = 3000
const l = 99
dim shared as string LangFile, Unmodded(Strings), Existant(Strings), Converted(Strings)
declare function lang(Text as string) as string
declare sub lang_select
function lang(Text as string) as string
	dim as ubyte ConvertID
	for ConvertID = 1 to Strings
		if Unmodded(ConvertID) = Text AND _
			Converted(ConvertID) < > "" then
			return Converted(ConvertID)
		end if
	next Strings
	return Text
end function
sub lang_select
	dim as ubyte ConvertID
	dim as integer Result
	input "Which language file";LangFile
	if LangFile < > "" then
		#IF defined(__FB_WIN32__) OR defined(__FB_DOS__)
		Result = open(MasterDir + "\" + LangFile + ".txt" for input as #l)
		#ELSE
		Result = open(MasterDir + "/" + LangFile + ".txt" for input as #l)
		#ENDIF
		if Result = 0 then
			for ConvertID = 1 to Strings
				Unmodded(ConvertID) = ""
				Converted(ConvertID) = ""
			next Strings
			for ConvertID = 1 to Strings
				line input #l, Unmodded(ConvertID)
				line input #l, Converted(ConvertID)
				if eof(l) then exit for
			next Strings
			close #l
		else
			print "Unable to open "+MasterDir+"/"+LangFile+" for reading."
			sleep
		end if
	end if
end sub