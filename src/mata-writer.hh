/** @file
 * @brief Writer of the `.mata` text of printed automata.
 */

#ifndef MATA_MATA_WRITER_HH_
#define MATA_MATA_WRITER_HH_

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace mata::parser {

/**
 * Collects the text of a printed automaton in a string, formatting the numbers with @c std::to_chars() instead of the
 *  locale machinery of a stream, which took most of the time of printing. The numbers are plain decimals whatever the
 *  format flags of the stream, as the parser reads them.
 */
class MataWriter {
  public:
	/// Collects the whole text for @c take().
	MataWriter() = default;
	/// Passes the text on to @p output chunk by chunk; see @c end_line() and @c flush().
	explicit MataWriter(std::ostream& output) : output_{&output} { text_.resize(2 * CHUNK_SIZE); }
	MataWriter(const MataWriter&) = delete;
	MataWriter(MataWriter&&) = delete;
	MataWriter& operator=(const MataWriter&) = delete;
	MataWriter& operator=(MataWriter&&) = delete;
	~MataWriter() = default;

	MataWriter& operator<<(const std::string_view text) {
		std::memcpy(room_for(text.size()), text.data(), text.size());
		size_ += text.size();
		return *this;
	}

	MataWriter& operator<<(const char ch) {
		*room_for(1) = ch;
		++size_;
		return *this;
	}

	template <std::unsigned_integral Number> MataWriter& operator<<(const Number number) {
		constexpr size_t MAX_DIGITS{std::numeric_limits<Number>::digits10 + 1};
		char* const digits{room_for(MAX_DIGITS)};
		size_ += static_cast<size_t>(std::to_chars(digits, digits + MAX_DIGITS, number).ptr - digits);
		return *this;
	}

	/// Ends a line, and passes the text on to the stream once it fills a chunk.
	void end_line() {
		*this << '\n';
		if (output_ != nullptr && size_ >= CHUNK_SIZE) { write_out(); }
	}

	/// Passes the rest of the text on to the stream, and flushes the stream once.
	void flush() {
		write_out();
		output_->flush();
	}

	/// The text collected without a stream.
	std::string take() {
		text_.resize(size_);
		size_ = 0;
		return std::move(text_);
	}

  private:
	/// Where to write @p num_of_chars more characters. The string is sized ahead and cut to the text at the end, so
	///  that writing a piece of a line is a copy rather than a call of @c std::string::append().
	char* room_for(const size_t num_of_chars) {
		if (text_.size() - size_ < num_of_chars) { text_.resize(std::max(2 * text_.size(), size_ + num_of_chars)); }
		return text_.data() + size_;
	}

	void write_out() {
		output_->write(text_.data(), static_cast<std::streamsize>(size_));
		size_ = 0;
	}

	static constexpr size_t CHUNK_SIZE{size_t{1} << 16};
	std::ostream* output_{nullptr};
	/// The text written is the first @c size_ characters.
	std::string text_{};
	size_t size_{0};
};

} // namespace mata::parser

#endif // MATA_MATA_WRITER_HH_
