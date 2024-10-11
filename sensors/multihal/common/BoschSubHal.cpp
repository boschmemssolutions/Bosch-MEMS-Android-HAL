/*
 * Copyright (C) 2023 Robert Bosch GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BoschSubHal.h"

#ifdef SUB_HAL_VERSION_2_0
::android::hardware::sensors::V2_0::implementation::ISensorsSubHal* sensorsHalGetSubHal(uint32_t* version) {
  static bosch::SensorsSubHal<::android::hardware::sensors::V2_1::subhal::implementation::SensorsSubHalV2_0> subHal;
  *version = SUB_HAL_2_0_VERSION;
  return &subHal;
}
#else
::android::hardware::sensors::V2_1::implementation::ISensorsSubHal* sensorsHalGetSubHal_2_1(uint32_t* version) {
  static bosch::SensorsSubHal<::android::hardware::sensors::V2_1::subhal::implementation::SensorsSubHalV2_1> subHal;
  *version = SUB_HAL_2_1_VERSION;
  return &subHal;
}
#endif
