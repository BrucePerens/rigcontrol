#!	/bin/sh
echo "const char gm_build_version[] = \"`git log -n 1 --format=format:%ci`\";" >$1
