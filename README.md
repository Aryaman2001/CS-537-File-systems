# CS-537-File-systems

Overview:

This is a group project that can be done in pairs of 2. In this project, you will recover all image files from an ext2 disk image. One of you should work on the content recovery of the jpg images (the first part), and the other one should work on image file name recovery (the second part).

Project Description:

Recently there have been lots of bank robberies. Just a few days ago, the police have identified a possible subject. The subject's motor vehicle contained, among other items, some hard drives (without the laptop). Apparently, the subject had deleted the files without reformatting the drives. Fortunately, the subject had never taken CS 537 before. This means that the subject does not know that most data and indeed most of the file control blocks still reside on the drives.

The police know that bank robbers usually take pictures of banks that they plan to rob. Thus, the police hire you, a file system expert, to be part of the forensics team attempting to reconstruct the contents of the disks. Each disk will be given to you in the form of a disk image (Links to an external site.) . A disk image is simply a file containing the complete contents and file system structures (For more, see "Ext2 image file" section below).

To catch the bad guy and prevent future robberies, your goal is to reconstruct all pictures (jpg files only) in the disk images and make the subject regret not taking CS 537 in the past. Of course, you may understand the regret of taking 537.

Note: A folder containing disk images was provided. Code written by my partner and I are in runscan.c.
