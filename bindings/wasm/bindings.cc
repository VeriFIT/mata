/** @file
 * @brief The JavaScript interface of the Mata playground.
 */

#include "playground.hh"

#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(mata_playground) {
	using mata::playground::Request;
	emscripten::value_object<Request>("Request")
		.field("aFormat", &Request::a_format)
		.field("aText", &Request::a_text)
		.field("bFormat", &Request::b_format)
		.field("bText", &Request::b_text)
		.field("operation", &Request::operation)
		.field("word", &Request::word)
		.field("alphabet", &Request::alphabet);
	emscripten::function("evaluate", &mata::playground::evaluate);
	emscripten::function("revision", &mata::playground::revision);
}
