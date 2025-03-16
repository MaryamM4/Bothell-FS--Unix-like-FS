#!/bin/bash

# Permissions only need to be set once.
#chmod +x restoreBFSDISK.sh     # Executable.
#chmod 444 BFSDISK-clean-backup # Read-only for safety. 

# Overwrite BFSDISK with backup's:
cp BFSDISK-clean-backup BFSDISK

echo "Restored BFSDISK."
echo