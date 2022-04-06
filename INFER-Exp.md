This is the document for our experiment for INFER. INFER is a static analysis tool deveoped by Meta: https://github.com/facebook/infer.
The infer version is v1.1.0.

```sh
$ infer --version
Infer version v1.1.0
Copyright 2009 - present Facebook. All Rights Reserved.
```

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

test1.c:12: error: Uninitialized Value
  Pulse found a potential uninitialized value `arg1` being read on on line 12.
  10. void caller() {
  11.     int arg, arg1;
  12.     callee(&arg, arg1);
                       ^
  13.     if (arg++){}
  14. }

test1.c:12: error: Uninitialized Value
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
Infer would miss this serious bug in the latent mode. We futher investigate why INFER ignores this bug, we list the summary here and describe our understandings, it might be not 100% correct. 

If we look at the memory model "mem" in the summary, v1 stands for the address of "arg", v5 stands for the pointer "arg" and the v6 stands for the object that "arg" points to. The "attrs" includes the properties that each variable should be, it does not contain the v6, which should be initialized properly before use. INFER fails to catch this bug in the latent mode mainly because of such incomplete summary.

```sh
--------------------------- 1 of 1 [nvisited:1 2 3 4] ---------------------------
PRE:
arg = val$1: ;
arg|->val$2:
POST 1 of 1:
arg = val$1:; return = val$3: ;
arg|->val$2:
----------------------------------------------------------------
Pulse: 1 pre/post(s)
#0: unsat:false,
    bo: { },
    citv: { },
    formula: known=true (no var=var) && true (no linear) && true (no atoms),
             pruned=true (no atoms),
             both=true (no var=var) && true (no linear) && true (no atoms)
    { roots={ &arg=v1 };
      mem  ={ v1 -> { * -> v5 }, v5 -> { * -> v6 } };
      attrs={ };}
    PRE=[{ roots={ &arg=v1 };
           mem  ={ v1 -> { * -> v5 }, v5 -> { * -> v6 } };
           attrs={ v1 -> { MustBeInitialized , MustBeValid  },
                   v5 -> { MustBeInitialized , MustBeValid  } };}]
    skipped_calls={ }
    Topl=[  ]

RacerD: 
Threads: NoThread, Locks: 0 
Accesses { } 
Ownership: Unowned 
Return Attribute: Nothing 
Attributes: { } 

Siof: (_|_,
{ })
Starvation: {thread= UnknownThread; return_attributes= Nothing;
             critical_pairs={ };
             scheduled_work= { };
             attributes= { }}
Uninitialised: 
 Pre: { } 
Post: { } 


Procedure: caller
void caller()
Analyzed
ERRORS: 
WARNINGS: 
FAILURE:NONE SYMOPS:14
Biabduction: phase= RE_EXECUTION
```

