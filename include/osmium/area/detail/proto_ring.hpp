#ifndef OSMIUM_AREA_DETAIL_PROTO_RING
#define OSMIUM_AREA_DETAIL_PROTO_RING

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013,2014 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <cassert>
#include <vector>

#include <osmium/osm/noderef.hpp>
#include <osmium/osm/ostream.hpp>
#include <osmium/area/segment.hpp>

namespace osmium {

    namespace area {

        namespace detail {

            /**
             * A ring in the process of being built by the Assembler object.
             */
            class ProtoRing {

                // nodes in this ring
                std::vector<osmium::NodeRef> m_nodes {};

                const NodeRefSegment& m_first_segment;

                // if this is an outer ring, these point to it's inner rings (if any)
                std::vector<ProtoRing*> m_inner {};

            public:

                ProtoRing(const NodeRefSegment& segment) :
                    m_first_segment(segment) {
                    add_location_end(segment.first_cw());
                    add_location_end(segment.second_cw());
                }

                const std::vector<osmium::NodeRef>& nodes() const {
                    return m_nodes;
                }

                void add_location_end(const osmium::NodeRef& nr) {
                    m_nodes.push_back(nr);
                }

                void add_location_start(const osmium::NodeRef& nr) {
                    m_nodes.insert(m_nodes.begin(), nr);
                }

                const osmium::NodeRef& first() const {
                    return m_nodes.front();
                }

                osmium::NodeRef& first() {
                    return m_nodes.front();
                }

                const osmium::NodeRef& last() const {
                    return m_nodes.back();
                }

                osmium::NodeRef& last() {
                    return m_nodes.back();
                }

                const osmium::Location top() const {
                    return first().location().y() > last().location().y() ? first().location() : last().location();
                }

                const osmium::Location bottom() const {
                    return first().location().y() < last().location().y() ? first().location() : last().location();
                }

                bool closed() const {
                    return m_nodes.front().location() == m_nodes.back().location();
                }

                bool is_outer() const {
                    int64_t sum = 0;

                    for (size_t i = 0; i < m_nodes.size(); ++i) {
                        size_t j = (i + 1) % m_nodes.size();
                        sum += static_cast<int64_t>(m_nodes[i].location().x()) * static_cast<int64_t>(m_nodes[j].location().y());
                        sum -= static_cast<int64_t>(m_nodes[j].location().x()) * static_cast<int64_t>(m_nodes[i].location().y());
                    }

                    return sum <= 0;
                }

                void swap_nodes(ProtoRing& other) {
                    std::swap(m_nodes, other.m_nodes);
                }

                void add_inner_ring(ProtoRing* ring) {
                    m_inner.push_back(ring);
                }

                const std::vector<ProtoRing*> inner_rings() const {
                    return m_inner;
                }

                void print(std::ostream& out) const {
                    out << "[";
                    bool first = true;
                    for (auto& nr : nodes()) {
                        if (!first) {
                            out << ',';
                        }
                        out << nr.ref();
                        first = false;
                    }
                    out << "]";
                }

                /**
                 * Merge other ring to end of this ring.
                 */
                void merge_ring(const ProtoRing& other, bool debug) {
                    if (debug) {
                        std::cerr << "        MERGE rings ";
                        print(std::cerr);
                        std::cerr << " to ";
                        other.print(std::cerr);
                        std::cerr << "\n";
                    }

                    if (last() == other.first()) {
                        for (auto ni = other.nodes().begin() + 1; ni != other.nodes().end(); ++ni) {
                            add_location_end(*ni);
                        }
                    } else {
                        exit(3); // XXX
                        assert(last() == other.last());
                        for (auto ni = other.nodes().rbegin() + 1; ni != other.nodes().rend(); ++ni) {
                            add_location_end(*ni);
                        }
                    }
                }

                ProtoRing* find_outer(bool debug) {
                    ProtoRing* ring = this;

                    while (!ring->is_outer()) {
                        const NodeRefSegment& segment = ring->m_first_segment;
                        if (debug) {
                            std::cerr << "      First segment is: " << segment << "\n";
                        }
                        NodeRefSegment* left = segment.left_segment();
                        assert(left);
                        if (debug) {
                            std::cerr << "      Left segment is: " << *left << "\n";
                        }
                        ring = left->ring();
                        assert(ring);
                        if (debug) {
                            std::cerr << "      Ring is ";
                            ring->print(std::cerr);
                            std::cerr << "\n";
                        }
                    }

                    return ring;
                }

            }; // class ProtoRing

            inline std::ostream& operator<<(std::ostream& out, const ProtoRing& ring) {
                ring.print(out);
                return out;
            }

            inline ProtoRing* combine_rings_end(ProtoRing& ring, std::list<ProtoRing>& rings, bool debug) {
                ProtoRing* ring_ptr = nullptr;
                osmium::Location location = ring.last().location();

                if (debug) {
                    std::cerr << "      combine_rings_end\n";
                }
                for (auto it = rings.begin(); it != rings.end(); ++it) {
                    if (&*it != &ring) {
                        if ((location == it->first().location())) { // || (location == it->last().location())) {
                            ring.merge_ring(*it, debug);
                            ring_ptr = &*it;
                            rings.erase(it);
                            return ring_ptr;
                        }
                    }
                }
                return ring_ptr;
            }

            inline ProtoRing* combine_rings_start(ProtoRing& ring, std::list<ProtoRing>& rings, bool debug) {
                ProtoRing* ring_ptr = nullptr;
                osmium::Location location = ring.first().location();

                if (debug) {
                    std::cerr << "      combine_rings_start\n";
                }
                for (auto it = rings.begin(); it != rings.end(); ++it) {
                    if (&*it != &ring) {
                        if ((location == it->last().location())) { // || (location == it->last().location())) {
                            ring.swap_nodes(*it);
                            ring.merge_ring(*it, debug);
                            ring_ptr = &*it;
                            rings.erase(it);
                            return ring_ptr;
                        }
                    }
                }
                return ring_ptr;
            }

        } // namespace detail

    } // namespace area

} // namespace osmium

#endif // OSMIUM_AREA_DETAIL_PROTO_RING
