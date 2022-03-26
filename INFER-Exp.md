This is the document for our experiment for INFER. INFER is a static analysis tool deveoped by Meta: https://github.com/facebook/infer.

Exp1: 
```C
//test1.c, this piece of code should be bug free.
void callee(int *arg, int arg1) {
    arg1 = 1;
    int a = 0;
    if (arg1) {
        *arg = a;
    }
}
void caller() {
    int arg, arg1;
    callee(&arg, arg1);
}
```

```sh
$ infer --pulse --enable-issue-type PULSE_UNINITIALIZED_VALUE -- clang -O0 -c test1.c 
Capturing in make/cc mode...
Found 1 source file to analyze in /opt/infer-linux64-v1.1.0/test/alias/infer-out
1/1 [################################################################################] 100% 80.559ms

test2.c:12: error: Uninitialized Value
  Pulse found a potential uninitialized value `arg1` being read on on line 12.
  10. void caller() {
  11.     int arg, arg1;
  12.     callee(&arg, arg1);
                       ^
  13.     if (arg++){}
  14. }

test2.c:12: error: Uninitialized Value
  The value read from arg1 was never initialized.
  10. void caller() {
  11.     int arg, arg1;
  12.     callee(&arg, arg1);
          ^
  13.     if (arg++){}
  14. }


Found 2 issues
                      Issue Type(ISSUED_TYPE_ID): #
        Uninitialized Value(UNINITIALIZED_VALUE): 1
  Uninitialized Value(PULSE_UNINITIALIZED_VALUE): 1
```
Infer is over aggressive and disallows uninitialized arguments passed to a function call, even there isn't any uninitialized use.

Exp2:
```C
//test2.c, this piece of code may have out-of-bound memory access.
void callee(int *arg) {
    int array[10] = {0};
    array[*arg] = 0;
}
void caller() {
    int arg;
    callee(&arg);
}
```

```sh
$ infer --pulse --enable-issue-type PULSE_UNINITIALIZED_VALUE_LATENT -- clang -O0 -c test2.c                                                                  
Capturing in make/cc mode...
Found 1 source file to analyze in /opt/infer-linux64-v1.1.0/test/alias/infer-out
1/1 [################################################################################] 100% 76.172ms

  No issues found  
```
Infer would miss this serious bug in the latent mode.

