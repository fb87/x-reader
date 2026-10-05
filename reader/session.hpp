#pragma once

#include "../core/geometry.hpp"
#include "../core/text.hpp"
#include "epub.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

/**
 * @brief The currently-open book's EPUB session and pagination, ported
 * from `app_load_epub`/`app_load_epub_chapter`/`app_turn_epub_chapter`/
 * `app_current_text`/`app_set_reading_progress` (app/app.c) and
 * `paginate()`/`turn()` (app/page_reader.c).
 *
 * This is the single most important correctness target in the whole
 * migration: it MUST call into `epub/epub.hpp` for real chapter text.
 * The architecture prototype this migration is based on got exactly
 * this wrong -- a fully-working, fully-tested EPUB parser that nothing
 * in the app ever called, so every book showed the same hardcoded
 * placeholder text regardless of which book was open. `current_text`
 * below falls back to `sample_text()` ONLY when no EPUB is open, the
 * same fallback the old app used, never as the default path.
 */
namespace reader {

inline constexpr int max_pages = 256;
inline constexpr std::uint32_t epub_manifest_max = 640;
inline constexpr std::uint32_t epub_spine_max = 640;
inline constexpr std::uint32_t epub_scratch_size = 80u * 1024u;
inline constexpr std::uint32_t epub_text_size = 64u * 1024u;

/** @brief Pride and Prejudice, Chapter 1 (public domain) -- shown only when no EPUB is open. */
inline const char* sample_text() {
  return
      "Chapter 1\n"
      "\n"
      "It is a truth universally acknowledged, that a single man in possession of a good "
      "fortune, must be in want of a wife.\n"
      "    However little known the feelings or views of such a man may be on his first "
      "entering a neighbourhood, this truth is so well fixed in the minds of the surrounding "
      "families, that he is considered the rightful property of some one or other of their "
      "daughters.\n"
      "    \"My dear Mr. Bennet,\" said his lady to him one day, \"have you heard that "
      "Netherfield Park is let at last?\"\n"
      "    Mr. Bennet replied that he had not.\n"
      "    \"But it is,\" returned she; \"for Mrs. Long has just been here, and she told me "
      "all about it.\"\n"
      "    Mr. Bennet made no answer.\n"
      "    \"Do you not want to know who has taken it?\" cried his wife impatiently.\n"
      "    \"You want to tell me, and I have no objection to hearing it.\"\n"
      "    This was invitation enough.\n"
      "    \"Why, my dear, you must know, Mrs. Long says that Netherfield is taken by a young "
      "man of large fortune from the north of England; that he came down on Monday in a "
      "chaise and four to see the place, and was so much delighted with it, that he agreed "
      "with Mr. Morris immediately; that he is to take possession before Michaelmas, and "
      "some of his servants are to be in the house by the end of next week.\"\n"
      "    \"What is his name?\"\n"
      "    \"Bingley.\"\n"
      "    \"Is he married or single?\"\n"
      "    \"Oh! Single, my dear, to be sure! A single man of large fortune; four or five "
      "thousand a year. What a fine thing for our girls!\"\n"
      "    \"How so? How can it affect them?\"\n"
      "    \"My dear Mr. Bennet,\" replied his wife, \"how can you be so tiresome! You must "
      "know that I am thinking of his marrying one of them.\"\n"
      "    \"Is that his design in settling here?\"\n"
      "    \"Design! Nonsense, how can you talk so! But it is very likely that he may fall in "
      "love with one of them, and therefore you must visit him as soon as he comes.\"\n"
      "    \"I see no occasion for that. You and the girls may go, or you may send them by "
      "themselves, which perhaps will be still better, for as you are as handsome as any of "
      "them, Mr. Bingley may like you the best of the party.\"\n"
      "    \"My dear, you flatter me. I certainly have had my share of beauty, but I do not "
      "pretend to be anything extraordinary now. When a woman has five grown-up daughters, "
      "she ought to give over thinking of her own beauty.\"\n"
      "    \"In such cases, a woman has not often much beauty to think of.\"\n"
      "    \"But, my dear, you must indeed go and see Mr. Bingley when he comes into the "
      "neighbourhood.\"\n"
      "    \"It is more than I engage for, I assure you.\"\n"
      "    \"But consider your daughters. Only think what an establishment it would be for one "
      "of them. Sir William and Lady Lucas are determined to go, merely on that account, for "
      "in general, you know, they visit no newcomers. Indeed you must go, for it will be "
      "impossible for us to visit him if you do not.\"\n"
      "    \"You are over-scrupulous, surely. I dare say Mr. Bingley will be very glad to see "
      "you; and I will send a few lines by you to assure him of my hearty consent to his "
      "marrying whichever he chooses of the girls; though I must throw in a good word for "
      "my little Lizzy.\"\n"
      "    \"I desire you will do no such thing. Lizzy is not a bit better than the others; and "
      "I am sure she is not half so handsome as Jane, nor half so good-humoured as Lydia. But "
      "you are always giving her the preference.\"\n"
      "    \"They have none of them much to recommend them,\" replied he; \"they are all silly "
      "and ignorant like other girls; but Lizzy has something more of quickness than her "
      "sisters.\"\n"
      "    \"Mr. Bennet, how can you abuse your own children in such a way? You take delight in "
      "vexing me. You have no compassion for my poor nerves.\"\n"
      "    \"You mistake me, my dear. I have a high respect for your nerves. They are my old "
      "friends. I have heard you mention them with consideration these last twenty years at "
      "least.\"\n";
}

/** @brief The currently-open book's EPUB session plus pagination of its current chapter. */
struct session {
  epub::context doc{};
  // Owned, not just referenced: epub::context only stores a pointer to this, and a chapter
  // load can happen long after the caller's own file_view (e.g. a local in open_book) would
  // have gone out of scope. Keeping the storage view here, with doc.storage pointing at it,
  // keeps it alive for the session's whole lifetime.
  epub::file_view storage_view{};
  std::array<epub::manifest_item, epub_manifest_max> manifest{};
  std::array<epub::spine_item, epub_spine_max> spine{};
  std::array<std::uint8_t, epub_scratch_size> scratch{};
  std::array<char, epub_text_size> text{};
  bool epub_open = false;
  bool cover_placeholder = false;
  std::uint16_t current_chapter = 0;
  char chapter_title_buf[96] = {0};

  // Pagination of the current chapter's text.
  std::array<std::uint32_t, max_pages> page_start{};
  int page_count = 0;
  int page = 0;
  int lines_per_page = 1;
  int total_pages = 0;  ///< set by the caller's layout step; see paginate()'s doc comment.
};

/** @brief Loads chapter `chapter`'s text; false on any parse failure or out-of-range index. */
inline bool load_chapter(session& s, std::uint16_t chapter) {
  if (!s.epub_open || chapter >= s.doc.spine_count) return false;
  std::uint32_t text_size_out = 0;
  if (epub::spine_text(s.doc, chapter, s.scratch.data(), s.scratch.size(), s.text.data(),
                       s.text.size(), text_size_out) != epub::status::ok) {
    return false;
  }
  s.cover_placeholder = s.text[0] == '\0';
  if (s.cover_placeholder) std::strcpy(s.text.data(), "Cover");
  s.current_chapter = chapter;
  return true;
}

/**
 * @brief Opens an EPUB and loads its first chapter. `fallback_title` is used only when
 * the EPUB's own `<dc:title>` is empty.
 */
inline bool open(session& s, const epub::file_view& storage, const char* fallback_title) {
  s.storage_view = storage;  // own a copy; see storage_view's doc comment on session.
  if (epub::open(s.doc, s.storage_view, s.scratch.data(), s.scratch.size(), s.manifest.data(),
                 static_cast<std::uint16_t>(s.manifest.size()), s.spine.data(),
                 static_cast<std::uint16_t>(s.spine.size())) != epub::status::ok) {
    return false;
  }
  s.epub_open = true;
  (void)fallback_title;  // the resolved title lives in s.doc.title; callers read that.
  return load_chapter(s, 0);
}

/** @brief Moves to the next/previous spine chapter; false at either end of the book. */
inline bool turn_chapter(session& s, int direction) {
  if (!s.epub_open) return false;
  const int next = static_cast<int>(s.current_chapter) + direction;
  if (next < 0 || next >= s.doc.spine_count) return false;
  return load_chapter(s, static_cast<std::uint16_t>(next));
}

/** @brief The current chapter's plain text, or the sample fallback when no EPUB is open. */
inline const char* current_text(const session& s) {
  return s.epub_open ? s.text.data() : sample_text();
}

/** @brief True when the current chapter is an image-only cover with no extractable text. */
inline bool is_cover_placeholder(const session& s) { return s.epub_open && s.cover_placeholder; }

/** @brief The current chapter's first line (its de facto title), or "Cover" if that's empty. */
inline const char* current_chapter_title(session& s) {
  const char* text = current_text(s);
  std::size_t n = 0;
  while (text[n] != '\0' && text[n] != '\n' && n + 1 < sizeof(s.chapter_title_buf)) ++n;
  std::memcpy(s.chapter_title_buf, text, n);
  s.chapter_title_buf[n] = '\0';
  return s.chapter_title_buf[0] != '\0' ? s.chapter_title_buf : "Cover";
}

inline std::uint16_t chapter_index(const session& s) { return s.current_chapter; }
inline std::uint16_t chapter_count(const session& s) {
  return s.epub_open ? s.doc.spine_count : 0;
}

/**
 * @brief Re-paginates the current chapter's text into `text_rect` using `font`, preserving
 * the reading position (the page containing the previous first character stays current).
 * Does NOT update `total_pages` -- the caller's layout step does that (reader_layout's old
 * role), matching the old app's split: `turn()`'s chapter-crossing branch repaginates
 * without refreshing total_pages, so progress briefly uses the prior chapter's page count
 * for one render. That's a real, if minor, quirk of the shipped app and is preserved
 * rather than silently fixed mid-migration.
 */
inline void paginate(session& s, const text::font& font, geometry::rect text_rect) {
  const std::uint32_t keep = s.page_count != 0 ? s.page_start[s.page] : 0;
  s.lines_per_page = std::max(text_rect.h / font.line_height, 1);

  const char* body = current_text(s);
  const std::size_t len = std::strlen(body);
  std::size_t off = 0;
  s.page_count = 0;
  while (off < len && s.page_count < max_pages) {
    s.page_start[s.page_count++] = static_cast<std::uint32_t>(off);
    for (int l = 0; l < s.lines_per_page && off < len; ++l) {
      std::size_t next = 0;
      text::wrap(font, body + off, text_rect.w, next);
      off += next;
    }
  }
  if (s.page_count == 0) s.page_start[s.page_count++] = 0;

  s.page = 0;
  for (int i = 0; i < s.page_count; ++i) {
    if (s.page_start[i] <= keep) s.page = i;
  }
}

/**
 * @brief Moves by `delta` pages, crossing a chapter boundary (and repaginating the new
 * chapter) when `delta` runs off the current one. Returns false only when already at the
 * start/end of the book. Matches the old `turn()` minus the display invalidation and
 * progress-bar mutation, which are the caller's (app page's) job.
 */
inline bool turn_page(session& s, int delta, const text::font& font, geometry::rect text_rect) {
  const int next = s.page + delta;
  if (next >= 0 && next < s.page_count) {
    s.page = next;
    return true;
  }
  if (!turn_chapter(s, delta)) return false;
  paginate(s, font, text_rect);
  s.page = delta > 0 ? 0 : s.page_count - 1;
  return true;
}

/** @brief Moves forward one page/chapter; false only at the end of the book. */
inline bool next_page(session& s, const text::font& font, geometry::rect text_rect) {
  return turn_page(s, +1, font, text_rect);
}

/** @brief Moves back one page/chapter; false only at the start of the book. */
inline bool previous_page(session& s, const text::font& font, geometry::rect text_rect) {
  return turn_page(s, -1, font, text_rect);
}

/**
 * @brief Overall book-reading progress as a percentage, ported from
 * `app_set_reading_progress`'s math: the completed-chapters fraction plus the
 * current chapter's within-chapter page fraction, divided by the chapter count.
 * Returns -1 when there's nothing meaningful to report (no EPUB, or no pages yet).
 */
inline int progress_percent(const session& s, int total_pages_in_chapter) {
  if (!s.epub_open || total_pages_in_chapter <= 0) return -1;
  const std::uint32_t chapters = chapter_count(s);
  if (chapters == 0) return -1;
  const std::uint32_t chapter_progress =
      static_cast<std::uint32_t>(s.page + 1) * 100u / static_cast<std::uint32_t>(total_pages_in_chapter);
  std::uint32_t progress = (static_cast<std::uint32_t>(s.current_chapter) * 100u + chapter_progress) / chapters;
  if (progress == 0 && (s.page > 0 || s.current_chapter > 0)) progress = 1;
  return static_cast<int>(std::min(progress, 100u));
}

}  // namespace reader
