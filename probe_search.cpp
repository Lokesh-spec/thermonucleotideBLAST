// ThermonucleotideBLAST
// 
// Copyright (c) 2007, Los Alamos National Security, LLC
// All rights reserved.
// 
// Copyright 2007. Los Alamos National Security, LLC. This software was produced under U.S. Government 
// contract DE-AC52-06NA25396 for Los Alamos National Laboratory (LANL), which is operated by Los Alamos 
// National Security, LLC for the U.S. Department of Energy. The U.S. Government has rights to use, 
// reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS ALAMOS NATIONAL SECURITY, 
// LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  
// If software is modified to produce derivative works, such modified software should be clearly marked, 
// so as not to confuse it with the version available from LANL.
// 
// Additionally, redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:
// 
//      * Redistributions of source code must retain the above copyright notice, this list of conditions 
//        and the following disclaimer.
//      * Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
//        and the following disclaimer in the documentation and/or other materials provided with the distribution.
//      * Neither the name of Los Alamos National Security, LLC, Los Alamos National Laboratory, LANL, 
//        the U.S. Government, nor the names of its contributors may be used to endorse or promote products 
//        derived from this software without specific prior written permission.
// 
// 
// THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND CONTRIBUTORS "AS IS" AND ANY 
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL SECURITY, LLC 
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
// OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "tntblast.h"

using namespace std;

bool hybrid(
	const list<oligo_info> &m_bind_minus,
	const list<oligo_info> &m_bind_plus,
	hybrid_sig &m_ret, 
	const hybrid_sig &m_sig, 
	const int &m_amp_start, const int &m_amp_stop)
{
	list<oligo_info>::const_iterator iter;
	
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Note that probe that have insertions or deletions relative to the target strand can show
	// up multiple times in the bind list (but with the same target start and stop range). Since
	// all of these entires are equivalent (and we return on the first valid match) there is no
	// need to remove them here (unlike the other hybrid function).
	///////////////////////////////////////////////////////////////////////////////////////////////
	
	// We will return the highest Tm probe match that we encounter
	m_ret.probe_tm = -1.0f;
	
	// Does the probe bind to the minus strand with a match that is between the primers?
	for(iter = m_bind_minus.begin();iter != m_bind_minus.end();iter++){
		
		// Only accept a probe with a higher Tm than we've already found (and are
		// contained in the amplicon).
		if( (iter->tm > m_ret.probe_tm) && (iter->loc_5 >= m_amp_start) && (iter->loc_3 <= m_amp_stop) ){
		
			m_ret.probe_range = make_pair(iter->loc_5, iter->loc_3);
			m_ret.probe_tm = iter->tm;
			m_ret.probe_dH = iter->dH;
			m_ret.probe_dS = iter->dS;
			m_ret.probe_mm = iter->num_mm;
			m_ret.probe_gap = iter->num_gap;
			m_ret.probe_strand = hybrid_sig::MINUS;
			m_ret.probe_align = iter->alignment;
		}
	}
	
	// Does the probe bind to the plus strand with a match that is between the primers?
	for(iter = m_bind_plus.begin();iter != m_bind_plus.end();iter++){
		
		// Only accept a probe with a higher Tm than we've already found (and are
		// contained in the amplicon).
		if( (iter->tm > m_ret.probe_tm) && (iter->loc_5 >= m_amp_start) && (iter->loc_3 <= m_amp_stop) ){
		
			m_ret.probe_range = make_pair(iter->loc_5, iter->loc_3);
			m_ret.probe_tm = iter->tm;
			m_ret.probe_dH = iter->dH;
			m_ret.probe_dS = iter->dS;
			m_ret.probe_mm = iter->num_mm;
			m_ret.probe_gap = iter->num_gap;
			m_ret.probe_strand = hybrid_sig::PLUS;
			m_ret.probe_align = iter->alignment;
		}
	}
	
	// If we found a match, m_ret.probe_tm will be greater than 0
	return (m_ret.probe_tm > 0.0f);
}

list<hybrid_sig> hybrid(DNAHash &m_hash, const pair<string, SEQPTR> &m_seq, 
	const hybrid_sig &m_sig, NucCruc &m_melt,
	const float &m_salt, const float &m_probe_strand, 
	const float &m_min_probe_tm, const float &m_max_probe_tm, 
	const float &m_min_probe_dg, const float &m_max_probe_dg, 
	const unsigned int &m_probe_clamp_5,
	const unsigned int &m_probe_clamp_3,
	const unsigned int &m_max_gap,
	const unsigned int &m_max_mismatch,
	const int &m_target_strand)
{
	const float probe_strand = m_probe_strand/m_sig.probe_degen;

	list<hybrid_sig> sig_list;
		
	m_melt.salt(m_salt);
	m_melt.strand(probe_strand);

	m_melt.set_query(m_sig.probe_oligo);
		
	list<oligo_info> bind;
	list<oligo_info>::const_iterator iter;
	
	// Does the probe bind to the minus strand?
	if(m_target_strand & Seq_strand_minus){

		bind_oligo_to_minus_strand(bind, 
			m_hash, m_seq.second, 
			m_sig.probe_oligo,
			m_melt,
			m_min_probe_tm, m_max_probe_tm,
			m_min_probe_dg, m_max_probe_dg,
			m_probe_clamp_5, m_probe_clamp_3,
			m_max_gap, m_max_mismatch);	
	}
	
	for(iter = bind.begin();iter != bind.end();iter++){

		// Make a copy of the current signature (including id)
		hybrid_sig tmp = m_sig;

		const int probe_start = iter->loc_5;
		const int probe_stop = iter->loc_3;
		
		if(probe_start > probe_stop){			
			throw __FILE__ ":hybrid: probe_start > probe_stop (0)";
		}

		const unsigned int probe_len = probe_stop - probe_start + 1;

		tmp.probe_tm = iter->tm;
		tmp.probe_dH = iter->dH;
		tmp.probe_dS = iter->dS;
		
		tmp.probe_mm = iter->num_mm;
		tmp.probe_gap = iter->num_gap;
		
		tmp.probe_range.first = probe_start;
		tmp.probe_range.second = probe_stop;
		tmp.amplicon_def = m_seq.first;

		tmp.probe_strand = hybrid_sig::MINUS;
		
		tmp.probe_align = iter->alignment;
						
		// ********************************************************
		// Copy the probe bases (taking the complement of the probe)
		SEQPTR ptr = SEQ_START(m_seq.second) + min( probe_stop, (int)SEQ_SIZE(m_seq.second) - 1);

		tmp.amplicon = string(probe_len, '-');

		for(unsigned int i = 0;i < probe_len;i++, ptr--){
			
			// Don't run past the end of the sequence
			if( ptr < SEQ_START(m_seq.second) ){
				break;
			}
			
			tmp.amplicon[i] = hash_base_to_ascii_complement(*ptr);
		}

		sig_list.push_back(tmp);
	}
	
	bind.clear();
		
	// Does the probe bind to the plus strand?
	// Does the probe bind to the minus strand?
	if(m_target_strand & Seq_strand_plus){
	
		bind_oligo_to_plus_strand(bind, 
			m_hash, m_seq.second, 
			m_sig.probe_oligo,
			m_melt,
			m_min_probe_tm, m_max_probe_tm,
			m_min_probe_dg, m_max_probe_dg,
			m_probe_clamp_5, m_probe_clamp_3,
			m_max_gap, m_max_mismatch);
	}
	
	for(iter = bind.begin();iter != bind.end();iter++){
		
		// Make a copy of the current signature (including id)
		hybrid_sig tmp = m_sig;

		const int probe_start = iter->loc_5;
		const int probe_stop = iter->loc_3;
		
		if(probe_start > probe_stop){
			throw __FILE__ ":hybrid: probe_start > probe_stop (1)";
		}
		
		const unsigned int probe_len = probe_stop - probe_start + 1;

		tmp.probe_tm = iter->tm;
		tmp.probe_dH = iter->dH;
		tmp.probe_dS = iter->dS;
		
		tmp.probe_mm = iter->num_mm;
		tmp.probe_gap = iter->num_gap;
		
		tmp.probe_range.first = probe_start;
		tmp.probe_range.second = probe_stop;
		tmp.amplicon_def = m_seq.first;
		
		tmp.probe_strand = hybrid_sig::PLUS;
		
		tmp.probe_align = iter->alignment;
		
		// ********************************************************
		// Copy the probe bases
		SEQPTR ptr = SEQ_START(m_seq.second) + max(0, probe_start);

		tmp.amplicon = string(probe_len, '-');

		for(unsigned int i = 0;i < probe_len;i++, ptr++){
			
			// Don't run past the end of the sequence
			if( (ptr - SEQ_START(m_seq.second) ) >= (int)SEQ_SIZE(m_seq.second) ){
				break;
			}
			
			tmp.amplicon[i] = hash_base_to_ascii(*ptr);
		}

		sig_list.push_back(tmp);
	}
	
	return sig_list;
}
