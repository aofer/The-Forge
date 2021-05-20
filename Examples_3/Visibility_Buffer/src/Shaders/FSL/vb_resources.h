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

#ifndef vb_resources_h
#define vb_resources_h

RES(SamplerState, textureFilter, UPDATE_FREQ_NONE, s0, binding = 2);
RES(Tex2D(float4), diffuseMaps[MATERIAL_BUFFER_SIZE], UPDATE_FREQ_NONE, t1, binding = 3);
RES(Buffer(uint), indirectMaterialBuffer, UPDATE_FREQ_PER_FRAME, t0, binding=1);
// RES(Buffer(MeshConstants), meshConstantsBuffer, UPDATE_FREQ_NONE, t1, binding=3 + MAX_TEXTURE_UNITS * 3);

#if defined(METAL)
	RES(Buffer(uint), mtlDrawID, UPDATE_FREQ_PER_DRAW, t0, binding=1);
	#define getDrawID() Get(mtlDrawID)[0]
#elif defined(DIRECT3D12)
	PUSH_CONSTANT(indirectRootConstant, b1)
	{
		DATA(uint, indirectDrawId, None);
	};
	#define getDrawID() indirectDrawId
#elif defined(VULKAN) || defined(ORBIS) || defined(PROSPERO)
    #define getDrawID() In.drawId
#endif

DECLARE_RESOURCES()

#endif /* vb_resources_h */
