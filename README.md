# sender
a mini program to model the way a sender would be implemented in C. also tests various type conversions and other fluff.
This only cares about the station number since the office code is not transmitted using revertive pulse signaling--just the 
last 4 of the phone number.

compile, then run with:
./sender <4 digit station number>
eg. ./sender 5678
