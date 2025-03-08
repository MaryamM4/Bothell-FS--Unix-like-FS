#!/bin/bash

# Permissions only need to be set once.
#chmod +x restoreBFSDISK.sh

# Set the permissions of BFSDISK-clean-backup to read-only for saftey.
#chmod -R 444 BFSDISK-clean-backup

# Overwrite BFSDISK with backup's:
cp -r BFSDISK-clean-backup/* ../src/BFSDISK/

echo "Restored BFSDISK."
echo