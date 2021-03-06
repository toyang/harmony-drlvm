;
;  Licensed to the Apache Software Foundation (ASF) under one or more
;  contributor license agreements.  See the NOTICE file distributed with
;  this work for additional information regarding copyright ownership.
;  The ASF licenses this file to You under the Apache License, Version 2.0
;  (the "License"); you may not use this file except in compliance with
;  the License.  You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
;  Unless required by applicable law or agreed to in writing, software
;  distributed under the License is distributed on an "AS IS" BASIS,
;  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;  See the License for the specific language governing permissions and
;  limitations under the License.
;
.class public org/apache/harmony/drlvm/tests/regression/h3225/MergeStackNegativeTest
.super java/lang/Object 

;
; Each instruction should have a fixed stack depth.
;
.method static test()V 
    .limit stack 2 
    .limit locals 1

    jsr LabelSub
    iconst_1
    jsr LabelSub
    return

LabelSub:
    astore 0 ; different depth
    ret 0
    
.end method

