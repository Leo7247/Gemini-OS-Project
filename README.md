# Introduction
This is my first Mini OS, is there anyone can give me suggestions for the OS name?  
This is a open source mini OS, it got Desktop, Explorer, Start, Notepad etc.
# Install Guide
If you have a **Windows Computer** : 
1. Download The QEMU Installer (if not already installed) From https://qemu.weilnetz.de/w64/qemu-w64-setup-20251224.exe.
2. Downlaod The GeminiOS.bin From Release.
3. Move your .bin file to your documents folder.
4. Open PowerShell and type ```cd Documents```
5. Type ```qemu-system-i386 -kernel GeminiOS.bin```

**Your QEMU Should Appear Now, If Not, Please Check Did You Follow All The Steps.**

If you have a **Mac computer** : 
1. Downlaod The GeminiOS.bin From Release.
2. Move your .bin file to your documents folder.
3. Open Terminal.
4. Install Homebrew (if not already installed):
```/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh```
5. Install QEMU (if not already installed) :
Run: ```brew install qemu```
6. Type ```cd ~/Documents``` in Terminal.
7. Type ```qemu-system-i386 -kernel GeminiOS.bin``` in Terminal.

**Your QEMU Should Appear Now, If Not, Please Check Did You Follow All The Steps.**

If You Have A **Linux Computer (Debian, Ubuntu, Raspberry Pi etc.)** : 
1. Downlaod The GeminiOS.bin From Release.
2. Move your .bin file to your Documents folder.
3. Open Terminal app.
4. Run ```sudo apt update
sudo apt install qemu-system-x86``` to install QEMU.
5. Type ```cd Documents```
6. Run ```qemu-system-i386 -kernel GeminiOS.bin``` to launch the QEMU and run GeminiOS.

**Your QEMU Should Appear Now, If Not, Please Check Did You Follow All The Steps.**

# MUST READ
1. The username is ```Leo```; Password is ```123```
2. You must follow all the install steps.
3. If you found some bugs, welcome to tell me.
4. You need to click ```Tab``` when you want to swich to password on login.
5. This OS can't connect to the network

# Screenshots
<img width="722" height="432" alt="Screenshot 2026-02-19 at 5 57 07 PM" src="https://github.com/user-attachments/assets/3f3c144c-1359-49c3-8c71-8102c4d3331b" />
<img width="721" height="433" alt="Screenshot 2026-02-19 at 5 59 23 PM" src="https://github.com/user-attachments/assets/7bcdf083-27a8-42f6-9c71-e15e4a56064e" />
<img width="719" height="438" alt="Screenshot 2026-02-19 at 6 00 59 PM" src="https://github.com/user-attachments/assets/20888448-599c-46fe-91df-2ec0905c6134" />
<img width="719" height="438" alt="Screenshot 2026-02-19 at 6 00 59 PM" src="https://github.com/user-attachments/assets/f02daa74-0daa-44c1-ac66-a5f489165a17" />
<img width="723" height="432" alt="Screenshot 2026-02-19 at 6 06 03 PM" src="https://github.com/user-attachments/assets/6b51ba4a-d24a-4776-96be-12d3a7dc62af" />

# Known Bugs
1. In the Paint app and the Notepad app, it duplicated the cusor shape.
2. In the calculator app, the numbers move right for 1 pixel.
** These Bugs Will Be Removed At The Future. **
