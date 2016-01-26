//
//  all.h
//  tsschecker
//
//  Created by tihmstar on 26.01.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#ifndef all_h
#define all_h

extern int dbglog;

#define info(a ...) printf(a)
#define log(a ...) if (dbglog) printf(a)

#endif /* all_h */
