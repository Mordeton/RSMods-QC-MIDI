#pragma once

#include <string>
#include "CCEnums.hpp"
#include "../Lib/Json/json.hpp"

namespace CrowdControl::Structs {
	struct Request {
		/**
		 * \brief The request ID, all responses to this request should contain the same ID value.
		 * \details These are not guaranteed to be sequential or increasing.
		 */
		uint32_t id;
		/**
		 * \brief The identifier of the requested effect.
		 * \details Only present on request types 0 (Test), 1(Start) and 2 (Stop).
		 */
		std::string code;
		/**
		 * \brief This field contains any parameters the user has selected.
		 */
		nlohmann::json parameters;
		/**
		 * \brief The displayable name of the viewer who requested the effect.
		 */
		std::string viewer;
		/**
		 * \brief The requested quantity of the effect, if applicable.
		 */
		int64_t quantity;
		/**
		 * \brief The requested duration of the effect, in milliseconds.
		 */
		int64_t duration;
		/**
		 * \brief The type of message. See RequestType enum.
		 */
		Enums::RequestType type;
	};

	struct Response {
		/**
		 * \brief The request ID, all responses to a request should contain the same ID value.
		 */
		int64_t id;
		/**
		 * \brief The identifier of the requested effect.
		 */
		std::string code;
		/**
		 * \brief Indicates the status of the effect request or class.
		 */
		Enums::EffectStatus status;
		/**
		 * \brief The requested duration of the effect, in milliseconds.
		 */
		int64_t timeRemaining;
		/**
		 * \brief Indicates the type of message. See ResponseType enum below.
		 */
		Enums::ResponseType type;
	};
}