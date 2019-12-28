#!/bin/bash

export HW1DIR=/media/sf_shared/VSCODE/oscode/HW1DIR
export HW1TF=targil1.txt

# Input: ${1} = Absolute file path (starts with '/') e.g. "/some/path/INPUTDATA.TXT"

# 1. Silently and recursively remove the subdirectory named HW1DIR (relative to the working directory)
rm -rf $HW1DIR

# 2. Create a subdirectory in the working directory, named HW1DIR ("replacing" the one you've just removed).
mkdir -p $HW1DIR

# 3. Set the permissions for the subdirectory: full access to the owner, read/execute to the group/others.
chmod 755 $HW1DIR

# 4. Change current directory to the new subdirectory that was just created.
cd $HW1DIR

# 5. Copy the text file specified by the command line argument to the current directory under the name defined by the HW1TF environment variable.
cp ${1} $HW1TF

# 6. Set the permissions for the file: full access to the owner, read-only to the group/others.
chmod 744 $HW1TF

# 7. Change current directory back to be the working directory.
cd ..

# 8. Invoke the hw1_subs executable (see Part I) on the copy of the file you just created. You choose the arguments supplied to the hw1_subs executable.
./hw1_subs sad happy
