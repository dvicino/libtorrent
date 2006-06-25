// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <algorithm>
#include <functional>
#include <rak/functional.h>

#include "data/chunk.h"

#include "block_transfer.h"
#include "block_list.h"
#include "exceptions.h"
#include "piece.h"

#include "transfer_list.h"

namespace torrent {

TransferList::iterator
TransferList::find(uint32_t index) {
  return std::find_if(begin(), end(), rak::equal(index, std::mem_fun(&BlockList::index)));
}

TransferList::const_iterator
TransferList::find(uint32_t index) const {
  return std::find_if(begin(), end(), rak::equal(index, std::mem_fun(&BlockList::index)));
}

void
TransferList::clear() {
  std::for_each(begin(), end(), rak::on(std::mem_fun(&BlockList::index), m_slotCanceled));
  std::for_each(begin(), end(), rak::call_delete<BlockList>());

  base_type::clear();
}

TransferList::iterator
TransferList::insert(const Piece& piece, uint32_t blockSize) {
  if (find(piece.index()) != end())
    throw internal_error("Delegator::new_chunk(...) received an index that is already delegated.");

  BlockList* blockList = new BlockList(piece, blockSize);
  
  m_slotQueued(piece.index());

  return base_type::insert(end(), blockList);
}

TransferList::iterator
TransferList::erase(iterator itr) {
  if (itr == end())
    throw internal_error("TransferList::erase(...) itr == m_chunks.end().");

  delete *itr;

  return base_type::erase(itr);
}

void
TransferList::finished(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    throw internal_error("TransferList::finished(...) got transfer with wrong state.");

  uint32_t index = transfer->block()->index();

  // Marks the transfer as complete and erases it.
  if (transfer->block()->completed(transfer))
    m_slotCompleted(index);
}

void
TransferList::hash_succeded(uint32_t index) {
  iterator blockListItr = find(index);

  if ((Block::size_type)std::count_if((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fun_ref(&Block::is_finished)) != (*blockListItr)->size())
    throw internal_error("TransferList::hash_succeded(...) Finished blocks does not match size.");

  erase(blockListItr);
}

struct transfer_list_compare_data {
  transfer_list_compare_data(Chunk* chunk, const Piece& p) : m_chunk(chunk), m_piece(p) { }

  bool operator () (const Block::failed_list_type::reference failed) {
    return m_chunk->compare_buffer(failed.first, m_piece.offset(), m_piece.length());
  }

  Chunk* m_chunk;
  Piece  m_piece;
};

void
TransferList::hash_failed(uint32_t index, Chunk* chunk) {
  iterator blockListItr = find(index);

  if (blockListItr == end())
    throw internal_error("TransferList::hash_failed(...) Could not find index.");

  if ((Block::size_type)std::count_if((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fun_ref(&Block::is_finished)) != (*blockListItr)->size())
    throw internal_error("TransferList::hash_failed(...) Finished blocks does not match size.");

  if ((*blockListItr)->attempt() == 0) {
    // First attempt, account for the new blocks.

    for (BlockList::iterator blockItr = (*blockListItr)->begin(), last = (*blockListItr)->end(); blockItr != last; ++blockItr) {
    
      Block::failed_list_type::iterator failedItr = std::find_if(blockItr->failed_list()->begin(), blockItr->failed_list()->end(),
                                                                 transfer_list_compare_data(chunk, blockItr->piece()));

      if (failedItr == blockItr->failed_list()->end()) {
        // We've never encountered this data before, make a new entry.
        char* buffer = new char[blockItr->piece().length()];

        chunk->to_buffer(buffer, blockItr->piece().offset(), blockItr->piece().length());

        blockItr->failed_list()->push_back(Block::failed_list_type::value_type(buffer, 1));

        // Count how many new data sets?

      } else {
        failedItr->second++;
      }
    }

    (*blockListItr)->inc_failed();

    // Retry with the most popular blocks.
    (*blockListItr)->set_attempt(1);
    retry_most_popular(*blockListItr, chunk);

    // Also consider various other schemes, like using blocks from
    // only/mainly one peer.

  } else {
    // Re-download the blocks.
    (*blockListItr)->clear_finished();
    (*blockListItr)->set_attempt(0);

    // Clear leaders when we want to redownload the chunk.
    std::for_each((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fun_ref(&Block::failed_leader));
  }
}

void
TransferList::retry_most_popular(BlockList* blockList, Chunk* chunk) {

  for (BlockList::iterator blockItr = blockList->begin(), last = blockList->end(); blockItr != last; ++blockItr) {
    
    Block::failed_list_type::iterator failedItr = std::max_element(blockItr->failed_list()->begin(), blockItr->failed_list()->end(),
                                                                   rak::less2(rak::mem_ptr_ref(&Block::failed_list_type::value_type::second),
                                                                              rak::mem_ptr_ref(&Block::failed_list_type::value_type::second)));

    if (failedItr == blockItr->failed_list()->end())
      throw internal_error("TransferList::retry_most_popular(...) No failed list entry found.");

    // Change the leader to the currently held buffer?

    chunk->from_buffer(failedItr->first, blockItr->piece().offset(), blockItr->piece().length());
  }

  m_slotCompleted(blockList->index());
}

}
