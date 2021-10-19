/* sam - SAM format writer.
    Copyright (C) 2020 Massimiliano Rossi
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/ .
*/
/*!
   \file sam.hpp
   \brief sam.hpp SAM format writer.
   \author Massimiliano Rossi
   \date 19/10/2021
*/

#ifndef _SAM_HH
#define _SAM_HH

#include <common.hpp>
#include <kpbseq.h>
#include <mapq.hpp>

////////////////////////////////////////////////////////////////////////////////
/// SAM flags
////////////////////////////////////////////////////////////////////////////////

#define SAM_PAIRED 1                      // template having multiple segments in sequencing
#define SAM_MAPPED_PAIRED 2               // each segment properly aligned according to the aligner
#define SAM_UNMAPPED 4                    // segment unmapped
#define SAM_MATE_UNMAPPED 8               // next segment in the template unmapped
#define SAM_REVERSED 16                   // SEQ being reverse complemented
#define SAM_MATE_REVERSED 32              // SEQ of the next segment in the template being reverse complemented
#define SAM_FIRST_IN_PAIR 64              // the first segment in the template
#define SAM_SECOND_IN_PAIR 128            // the last segment in the template
#define SAM_SECONDARY_ALIGNMENT 256       // secondary alignment
#define SAM_FAILS_CHECKS 512              // not passing filters, such as platform/vendor quality controls
#define SAM_DUPLICATE 1024                // PCR or optical duplicate
#define SAM_SUPPLEMENTARY_ALIGNMENT 2048  // supplementary alignment

////////////////////////////////////////////////////////////////////////////////

typedef struct sam_t{
    // bool reverse = false; // The read is the reverse complement of the original 
    const kseq_t *read = nullptr; // The read of the SAM entry
                            // Contains: QNAME, SEQ, and QUAL
    size_t flag   = 4;    // FLAG: bitwise FLAG
    size_t pos    = 0;    // POS: 1-based leftmost mapping POSition
    size_t mapq   = 255;  // MAPQ: MAPping Quality
    size_t pnext  = 0;    // PNEXT: Position of the mate/next read
    long long int tlen   = 0;        // TLEN: Position of the mate/next read

    std::string rname = "*"; // RNAME: Reference sequence NAME
    std::string cigar = "*"; // CIGAR: CIGAR string
    std::string rnext = "*"; // RNEXT: Reference name of the mate/next read

    size_t as = 0; // AS: Alignment score generated by aligner
    size_t nm = 0; // NM: Edit distance to the reference
    size_t zs = 0; // ZS: Second best score

    std::string md = ""; // MD: String encoding mismatched and deleted reference bases

    size_t rlen = 0; // Length of the match in the referenc. Requiredd to compute TLEN

    // Quind the sam_t struct for a read
    sam_t(const kseq_t* read_)
    {
        read = read_;
    }

} sam_t;


inline void write_sam(FILE *out, const sam_t s)
{
    fprintf(out, "%s\t", s.read->name.s);         // QNAME
    fprintf(out, "%d\t", s.flag);                 // FLAG
    fprintf(out, "%s\t", s.rname.c_str());        // RNAME
    fprintf(out, "%d\t", s.pos);                  // POS
    fprintf(out, "%d\t", s.mapq);                 // MAPQ
    fprintf(out, "%s\t", s.cigar.c_str());        // CIGAR
    fprintf(out, "%s\t", s.rnext.c_str());        // RNEXT
    fprintf(out, "%d\t", s.pnext);                // PNEXT
    fprintf(out, "%d\t", s.tlen);                 // TLEN
    fprintf(out, "%s\t", s.read->seq.s);          // SEQ

    if (s.read->qual.s)                           // QUAL
        fprintf(out, "%s", s.read->qual.s);
    else
        fprintf(out, "*");

    // Optional TAGs
    if (!(s.flag & SAM_UNMAPPED))
    {
        fprintf(out, "\tAS:i:%d", s.as);            // AS
        fprintf(out, "\tNM:i:%d", s.nm);            // NM
        if (s.zs > 0)
        fprintf(out, "\tZS:i:%d", s.zs);          // ZS
        fprintf(out, "\tMD:Z:%s\n", s.md.c_str());  // MD
    }else
        fprintf(out, "\n");
}

// Adapted from https://github.com/mengyao/Complete-Striped-Smith-Waterman-Library/blob/master/src/main.c
  void write_sam(const int32_t score,
                 const int32_t score2,
                 const int32_t min_score,
                 size_t ref_pos,
                 const char *ref_seq_name,
                 const kseq_t *read,
                 int8_t strand, // 0: forward aligned ; 1: reverse complement aligned
                 FILE *out,
                 std::string &cigar,
                 std::string &md,
                 size_t mismatches,
                 const char *r_next,  // Reference sequence name of the primary alignment of the NEXT read in the template.
                 const size_t p_next, // 0-based Position of the primary alignment of the NEXT read in the template.
                 const int32_t t_len  // TLEN: signed observed Template LENgth.
  )
  {
    // Sam format output
    fprintf(out, "%s\t", read->name.s);
    if (score == 0)
      fprintf(out, "4\t*\t0\t255\t*\t*\t0\t0\t*\t*\n");
    else
    {
      int32_t c, p;
      uint32_t mapq = -4.343 * log(1 - (double)abs(score - score2) / (double)score);
      // mapq = (uint32_t)(mapq + 4.99);
      // mapq = mapq < 254 ? mapq : 254;
    //   uint32_t mapq = compute_mapq(score,score2,min_score,read->seq.l); 
      if (strand)
        fprintf(out, "16\t");
      else
        fprintf(out, "0\t");
      // TODO: Find the correct reference name.
      fprintf(out, "%s\t%d\t%d\t", ref_seq_name, ref_pos + 1, mapq);
      fprintf(out, "%s\t", cigar.c_str());
      fprintf(out, "%s\t%d\t%d\t", r_next, p_next + 1, t_len);
      // fprintf(out, "\t*\t0\t0\t");
      fprintf(out, "%s", read->seq.s);
      fprintf(out, "\t");
      if (read->qual.s && strand)
      {
        for (p = read->qual.l - 1; p >= 0; --p)
          fprintf(out, "%c", read->qual.s[p]);
      }
      else if (read->qual.s)
        fprintf(out, "%s", read->qual.s);
      else
        fprintf(out, "*");
      fprintf(out, "\tAS:i:%d", score);
      fprintf(out, "\tNM:i:%d", mismatches);
      if (score2 > 0)
        fprintf(out, "\tZS:i:%d", score2);
      fprintf(out, "\tMD:Z:%s\n", md.c_str());
    }
  }

#endif /* end of include guard: _SAM_HH */