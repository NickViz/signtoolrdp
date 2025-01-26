# signtoolrdp

Detours library that hides RDP session ID for applications that use USB keys.

# Problem

Microsoft Smart Card subsystem blocks access to any locally attached usb tokens
or smart card readers in a remote session. Theoretically, you should attach required
devices on the client side, but it often is not acceptable. This problem is visible
using such protocols as RDP or PCoIP.

See more details at https://lifayk.blogspot.com/2012/07/windows-smart-card-subsystem-and-remote.html

# Solution

Instead of patching the WinSCard.dll we add to the target application (signtool.exe in my case)
a dll that Detours the WinStationGetCurrentSessionCapabilities Windows function from WinSCard.dll
and we respond the session ID 0, pretending to work locally on the system. That solves the problem above. 
The idea is taken here: https://stackoverflow.com/questions/15906740/how-to-use-an-ev-code-signing-certificate-on-a-virtual-machine-to-sign-an-msi

# Howto

* Get the latest signtoolrdp project: git clone https://github.com/NickViz/signtoolrdp.git
* Get the latest Detours library: git clone https://github.com/microsoft/Detours.git
* Get the VisualStudio2022 (any edition, community is OK)
* Build Detours library:
  - Run "x86 Native Tools Command Prompt for VS 2022" or "x64 Native Tools Command Prompt for VS 2022" from start menu.
  - Navigate to the Detours library path. For instance: cd /d D:\Projects\Detours 
  - Build library: nmake
  - Make sure that in the Detours folder the lib.X86 or lib.X64 folder(s) appeared.
* Open signtoolrdp solution and build necessary target (Release) of the signtoolrdp.dll.
* Download the PE Bear viewr/editor: https://github.com/hasherezade/pe-bear
* Get the application you want to patch (in my case it's the signtool.exe).
* Check if it's 32- or 64-bit application.
  - I simply run it in the RDP session and when it complains that no smart card is available - checked it in the Task Manager. In my case it was 64-bit app.
* Place application and corresponding dll to the same folder.  
* Open PE-Bear, load application and navigate to the "Imports" tab (see screen_shot1.png)
* Click on "Add Imports via Wizard" item and fill in the DLL: and Function: fields by signtoolrdp.dll and #0 respectively.
* Mark "Use a new section" checkbox (see screen_shot2.png) and click on Add and then Save buttons.  
* Check that the signtoolrdp.dll was added (see screen_shot3.png).
* Save the new file:
  - Right mouse click on the application name (top-left corner)
  - Select "Save the executable as.." option
  - Select a new name, for example signtoolrdp.exe and save it.
* Check that the patched version works after patching. Run it, it should work as before.
* Move patched version and corresponding dll to the target VM and check the signing process. Now it should access USB key.

# Troubleshooting

* If application doesn't run after the patch:
  - Check that you selected the right dll bit-ness.
  - Check that you selected the Release dll.
  - Check that you installed the vc_redist from the Visual Studio 2022.
  - Try to patch application again, selecting #0, #1 or #2 in the Function: field.
* If application doesn't see the USB key in RDP session:
  - Run the DebugView from Sysinternals: https://learn.microsoft.com/en-us/sysinternals/downloads/debugview 
  - Run the application. In the DebugView you should see the lines:
DllMain Entry 00000001 
Starting Detour API Calls 
Successfully Detoured API Calls  
DllMain Entry 00000000 
Successfully reverted API Calls  
  - If you see "Successfully Detoured API Calls", but the app doesn't see the key(s) - try to enable the sessionID detour:
  - In DllMain.cpp find line 23 and uncomment it: #define DETOUR_SESSION_ID
  - Build dll, patch application and try again.
  
# Security

You should understand that this solution reduces a bit the security of the USB keys and violates Microsoft
rules about USB keys in RDP session.

I do not provide any binary files, you must to compile everything yourself.
The provided code is short enough to be inspected and doesn't contain any undeclared functionality.
Detours is a well-known library from Microsoft, so it's rather safe to use it.

In common - it's a workaround, use it on you own risk.
I use in in my own work, I saw many requests for that, so I decided to share it.

Cheers,
Nikolai
