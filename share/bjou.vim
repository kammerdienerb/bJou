" Vim syntax file
" Language:	bJou
" Maintainer:	Brandon Kammerdiener (kammerdienerb@gmail.com)
" Last Change:  2018 Apr 27

if exists('b:current_syntax')
    finish
endif

syn case match

" Keywords

syn keyword     bjouFlowControl     return if else while do for foreach in match with and or break continue
syn keyword     bjouTypes           u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 f128 void bool char short int long float double unsigned byte string file list dict
syn keyword     bjouValWords        not true false nothing some
syn keyword     bjouKeywords        const macro proc __no_mangle__ extern externvar type extends abstract this ref enum print new mut delete as import include module using sizeof bshl bshr band bor bxor bneg

hi def link     bjouFlowControl     Statement
hi def link     bjouTypes           Type
hi def link     bjouValWords        Constant 
hi def link     bjouKeywords        Keyword

" Identifiers
syn match       bjouIdentifier      /[_a-zA-Z][_a-zA-Z0-9]*'*/
hi def link     bjouIdentifier      Identifier

" Macros
syn match       bjouMacro           /\\[_a-zA-Z][_a-zA-Z0-9]*'*/
hi def link     bjouMacro           Macro 

" Comments
syn match       bjouComment         /#.*/
hi def link     bjouComment         Comment

" Numbers
syn match       bjouInt             /-\?\d\+/
syn match       bjouFloat           /-\?\d\+\.\d\+/
hi def link     bjouInt             Number
hi def link     bjouFloat           Number

" Characters
syn region      bjouCharacter       start=+'+ skip=+\\'+ end=+'+
syn match       bjouCharacter       /'\\\\'/
hi def link     bjouCharacter       Character

" Strings
syn region      bjouString          start=+"+ skip=+\\"+ end=+"+
hi def link     bjouString          String


let b:current_syntax = 'bjou'
