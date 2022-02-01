#!/bin/sh

CLANG=${CLANG:-clang}
OFILE=`echo $* | sed -e 's/^.* \(.*\.o\) .*$/\1/'`
if [ "x$OFILE" != x -a "$OFILE" != "$*" ] ; then
    #$CLANG -emit-llvm $* -O0 -g -fno-short-wchar >/dev/null 2>&1
    $CLANG -emit-llvm "$@" -O0 -g -fno-short-wchar >/dev/null 2>&1 > /dev/null
    if [ -f "$OFILE" ] ; then
        BCFILE=`echo $OFILE | sed -e 's/o$/llbc/'`
        #file $OFILE | grep -q "LLVM IR bitcode" && mv $OFILE $BCFILE || true
        if [ `file $OFILE | grep -c "LLVM IR bitcode"` -eq 1 ]; then
            mv $OFILE $BCFILE
        else
            touch $BCFILE
        fi
    fi
fi
exec $CLANG "$@"
