///<------------------------------------------------------------------------------
//< @file:   tokit\tokit\generator.cpp
//< @author: ������
//< @date:   2014��9��25�� 14:46:17
//< @brief:  
//< Copyright (c) 2014 Tokit. All rights reserved.
///<------------------------------------------------------------------------------

#include "generator.h"
#include "parser.h"
#include "file_util.h"

void generator::echo_errs()
{
    echoutil::echo_errvec(m_errvec);
    m_errvec.clear();
}
