/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#ifndef RESOURCES_H
#define RESOURCES_H

// UPDATE_FREQ_NONE
RES(Tex2D(float4), RightText, UPDATE_FREQ_NONE, t1, binding = 1);
RES(Tex2D(float4), LeftText,  UPDATE_FREQ_NONE, t2, binding = 2);
RES(Tex2D(float4), TopText,   UPDATE_FREQ_NONE, t3, binding = 3);
RES(Tex2D(float4), BotText,   UPDATE_FREQ_NONE, t4, binding = 4);
RES(Tex2D(float4), FrontText, UPDATE_FREQ_NONE, t5, binding = 5);
RES(Tex2D(float4), BackText,  UPDATE_FREQ_NONE, t6, binding = 6);
RES(SamplerState,  uSampler0, UPDATE_FREQ_NONE, s0, binding = 7);

// UPDATE_FREQ_PER_FRAME
#ifndef MAX_PLANETS
    #define MAX_PLANETS 20
#endif
CBUFFER(uniformBlock, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
    DATA(float4x4, mvp, None);
    DATA(float4x4, toWorld[MAX_PLANETS], None);
    DATA(float4, color[MAX_PLANETS], None);

    // Point Light Information
    DATA(float3, lightPosition, None);
    DATA(float3, lightColor, None);
};

#endif