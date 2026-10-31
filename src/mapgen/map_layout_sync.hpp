#pragma once

#include <vector>

#include "map_layout_chunkizer.hpp"
#include "resources.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "net/outmessage.hpp"

namespace mg
{

void WriteMapLayoutChunk(net::OutMessage& msg, const MapLayoutChunk& map_chunk);
bool ReadMapLayoutChunk(net::InMessage& msg, MapLayoutChunk& map_chunk, const ResourceSet& res);

MapLayoutChunk ReencodeMapLayoutChunk(std::vector<char>& buffer, const MapLayoutChunk& map_chunk,
                                      const ResourceSet& res);

} // namespace mg
