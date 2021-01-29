// Copyright 2021 Tiro ehf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef TIRO_SPEECH_SRC_API_VALIDATION_H_
#define TIRO_SPEECH_SRC_API_VALIDATION_H_

#include <grpcpp/grpcpp.h>

#include <string>
#include <vector>

#include "proto/tiro/speech/v1alpha/speech.pb.h"
#include "src/kaldi-model.h"

namespace tiro_speech {

/** \brief vector of pairs {field, error message}. Empty vector is a valid
 *         message.
 */
using MessageValidationStatus =
    std::vector<std::pair<std::string, std::string>>;

/** \brief Validate Protobuf messages
 *
 * Validate() will read the message and validate each field and each subfield.
 * Any validation errors found are returned as a vector of pairs, where p.first
 * is the field and p.second is the error message. Fields are named in form:
 * <top level field>[.<1st level subfield>[.<2nd level subfield>.]] and so on.
 *
 * By default Validate() won't validate fields that depend on the models being
 * used. But, Validate() takes an optional ptr to a map of models.  If set,
 * fields that depend on the model used (or requested) are also validated.
 */
MessageValidationStatus Validate(
    const tiro::speech::v1alpha::RecognitionConfig& config,
    const KaldiModelMap* models = nullptr);
MessageValidationStatus Validate(
    const tiro::speech::v1alpha::RecognizeRequest& request,
    const KaldiModelMap* models = nullptr);

/** \brief Convert validation errors to gRPC errors
 *
 *  This will convert any validation error in \p errors to a
 *  google::rpc::FieldViolation
 *
 *  \param errors List of fields with errors and the
 *  \return A gRPC OK status if errors is empty, otherwise this adds a
 *          BadRequest with FieldViolations
 */
grpc::Status ErrorVecToStatus(const MessageValidationStatus& errors);

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_API_VALIDATION_H_
