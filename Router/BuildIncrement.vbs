Option Explicit 

Const FOR_READING = 1 
Const FOR_WRITING = 2 
Const FOR_APPENDING = 8 

Const FVERSION = "FILEVERSION "
Const PVERSION = "PRODUCTVERSION "
Const AVERSION = "FileVersion"", "
Const AFVERSION = "ProductVersion"", "

'Const FILENAME = "AssemblyInfo.cs"
Const FILENAME = "aaa.txt"

Function ReplaceAVesrion(fC,start)
   dim buildNr
   
   posInfo = InStr(start,fC, AVERSION)

   if (posInfo>0) then
      sPos = posInfo+Len(AVERSION)+1
      str1 = Mid(fC,sPos,Instr(sPos,fC,"""")-sPos)
  
      buildNr = Right(str1,Len(str1)-InStrRev(str1,"."))
      buildNr = buildNr + 1
      
      fC = Replace(fC,str1,Left(str1,InStrRev(str1,".")) & buildNr,1)  
      
      posInfo = InStr(start,fC, FVERSION)         
      if (posInfo>0) then
      
         sPos = posInfo+Len(FVERSION)
         str1 = Mid(fC,sPos,Instr(sPos,fC,chr(10))-sPos)     
         
         fC = Replace(fC,str1,Left(str1,InStrRev(str1,",")) & buildNr,1)       
      end if
   end if 
   
   ReplaceAVesrion = fC    
end Function

Dim objFso 
Dim objFile 
dim fC
Dim posInfo
Dim str1
Dim sPos
Dim args
Dim argv
Dim file_name

set args = WScript.Arguments
argv = args.Count

if(argv = 0)then
   msgbox "No input file"
    WScript.Quit 1
end if

file_name = FILENAME
file_name = args.Item(0)

Set objFso = CreateObject("Scripting.FileSystemObject")

if(not objFso.FileExists(file_name))then
   msgbox "File doesnt exist"
   WScript.Quit 1   
end if
 
Set objFile = objFso.OpenTextFile(file_name,FOR_READING,0,True) 
fC = objFile.ReadAll              
objFile.Close

ReplaceAVesrion fC,1

Set objFile = objFso.OpenTextFile(file_name, FOR_WRITING,0,True) 
objFile.Write fC 
objFile.Close

Set objFile = Nothing 
Set objFso = Nothing


