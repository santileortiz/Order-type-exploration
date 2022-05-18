//
// Tom Sawyer Software
// Copyright 2007-2022
// All rights reserved.
//
// www.tomsawyer.com
//

#if !defined(KEYCODES_H)

#if __APPLE__

#define KEY_1 26
#define KEY_2 27
#define KEY_3 28
#define KEY_4 29
#define KEY_5 31
#define KEY_6 30
#define KEY_7 34
#define KEY_8 36
#define KEY_9 33
#define KEY_0 37

#define KEY_Q 20
#define KEY_P 43
#define KEY_C 16
#define KEY_V 17
#define KEY_N 53
#define KEY_M 54

#define KEY_UP_ARROW 134
#define KEY_DOWN_ARROW 133
#define KEY_LEFT_ARROW 131
#define KEY_RIGHT_ARROW 132

#define KEY_TAB 56
#define KEY_BACKSPACE 59
#define KEY_ENTER 44
#define KEY_ESC 61

#elif __linux__

#define KEY_1 10
#define KEY_2 11
#define KEY_3 12
#define KEY_4 13
#define KEY_5 14
#define KEY_6 15
#define KEY_7 16
#define KEY_8 17
#define KEY_9 18
#define KEY_0 19

#define KEY_Q 24
#define KEY_P 33
#define KEY_C 54
#define KEY_V 55
#define KEY_N 57
#define KEY_M 58

#define KEY_UP_ARROW 111
#define KEY_DOWN_ARROW 116
#define KEY_LEFT_ARROW 113
#define KEY_RIGHT_ARROW 114

#define KEY_TAB 23
#define KEY_BACKSPACE 22
#define KEY_ENTER 36
#define KEY_ESC 9

#endif

#define KEYCODES_H
#endif
