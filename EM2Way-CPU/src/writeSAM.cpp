#include  "writeSAM.h"


inline void writeSAM(uint64_t &bytes, char *buffer, void *a, uint64_t a_bytes){   

    memcpy(&buffer[bytes], a, a_bytes);
	bytes += a_bytes;
}

inline void writeSAMCStr(uint64_t &bytes, char *buffer, void *a, uint64_t a_bytes){ 
    
    char * a_char = (char *)a;
    uint16_t end_idx = a_bytes;
    
    // custom loop to get the first space
    for (uint64_t i = 0; i < a_bytes; i++){
        if (a_char[i] == ' '){
            end_idx = i;
            break;
        }
    }
    memcpy(&buffer[bytes], a, sizeof(char)*end_idx);
    bytes += end_idx;
}


inline void writeSAMTab(uint64_t &bytes, char *buffer){
    
    char t = '\t';

    memcpy(&buffer[bytes], &t, 1);
	bytes += 1;
}


inline void writeSAMVal(uint64_t &bytes, char *buffer, void *a, uint64_t a_bytes){

    char tmp_buffer [a_bytes];
    memset(tmp_buffer, 0, sizeof(tmp_buffer));

    // convert to string 
    sprintf(tmp_buffer, "%u", *(uint32_t*)a);
    
    // write each char string element
    writeSAM(bytes, buffer, (void *)tmp_buffer, sizeof(char));
    for (uint8_t i = 1; i<sizeof(tmp_buffer); i++){
        if (tmp_buffer[i] != 0){
            writeSAM(bytes, buffer,  (void *) (tmp_buffer + i), sizeof(char));
        }
    }
}


inline void setSAMOptional(uint64_t &bytes, char *buffer, read2Bit_t & read, uint32_t & suppress_aligned){

    // MD:Z:<S> For aligned reads, 
    //          <S> is a string representation of the 
    //          mismatched reference bases in the alignment
    // XA:i:<N> Aligned read belongs to stratum <N>.
    // XM:i:<N> the read’s alignments were suppressed
    // NM:i:<V> edit distance
    
    char tmp_char = '0';
    char tmp_cstr[] = "MD:Z:";
    uint32_t tmp_val = read.seq_len;

    writeSAM(bytes, buffer, &tmp_cstr, sizeof(tmp_cstr)-1);
    writeSAMVal(bytes, buffer, &tmp_val, MATCH_BIT);
    writeSAMTab(bytes, buffer);


    strcpy(tmp_cstr, "XA:i:");
    writeSAM(bytes, buffer, &tmp_cstr, sizeof(tmp_cstr)-1);
    writeSAM(bytes, buffer, &tmp_char, 1);
    writeSAMTab(bytes, buffer);


    if (suppress_aligned > 0){    
        strcpy(tmp_cstr, "XM:i:");
        writeSAMVal(bytes, buffer, &suppress_aligned, POS_BIT);
        writeSAMTab(bytes, buffer);
    }

    
    strcpy(tmp_cstr, "NM:i:");
    writeSAM(bytes, buffer, &tmp_cstr, sizeof(tmp_cstr)-1);
    writeSAM(bytes, buffer, &tmp_char, 1);
}



inline void setSAMFlag(uint64_t &bytes, char *buffer, read2Bit_t & read){

    int flag = 0;
    
    // 0x4
    //if ((read.is_f_align || read.is_b_align) != 1){
    if ((read.isaligned_fw || read.isaligned_bw) != 1){
        flag += 4;
    }

    // 0x10
    if (read.isaligned_fw){
        flag += 16;
    }

    writeSAMVal(bytes, buffer, &flag, 4);
}

inline char getReverseComplement(char b){

    char rtn_char;
    switch(b){
        case 'A':
            rtn_char = 'T';
            break;
        case 'C':
            rtn_char = 'G';
            break;
        case 'G':
            rtn_char = 'C';
            break;
        case 'T':
            rtn_char = 'A';
            break;
        
        case 'a':
            rtn_char = 't';
            break;
        case 'c':
            rtn_char = 'g';
            break;
        case 'g':
            rtn_char = 'c';
            break;
        case 't':
            rtn_char = 'a';
            break;
        default:
            rtn_char = 'N';
    }
    return rtn_char;
}

int getChrsIdx(uint32_t pos, std::vector<chr_t> &chrs){

    if (chrs.size() == 0) return 0;
    // binary search
    uint16_t chrs_num = chrs.size();
    uint16_t mid = (chrs_num  - 1)/ 2;
    
    uint16_t start = 0;
    uint16_t end = chrs_num - 1;

    uint16_t sel = 0;

    while(start <= end){

        if (pos > chrs[mid].begin){
            sel = mid;

            start = mid + 1;
            end = end;
            mid = (end - start) / 2 + start; 
        }
        else if (pos < chrs[mid].begin){
            start = start;
            end = (mid == 0)? mid: mid - 1;
            mid = (end - start) / 2 + start; 

            sel = end;
        }
        else if (pos == chrs[mid].begin){
            return mid;
        }

        if (start >= end){
            if (pos >= chrs[end].begin){
                return end;
            }
            else{
                return sel;
            }
        }
    }
    return -1;
}

void writeSAMFields(uint64_t &bytes, 
                    char *   buffer,
                    std::vector<chr_t> &chrs,
                    read2Bit_t & read,
                    uint32_t & suppress_aligned,
                    uint32_t * aligned_sai_b,
                    uint32_t * aligned_sai_f,
                    bool is_b,
                    uint32_t sai_i){
                    
    uint32_t tmp_val = 0;
    char tmp_char = '0';
    char seq_revc[MAX_READ_LEN+1];
    int chr_idx;
    //cerr<<"chk_pt1\n";

    // 1. QNAME
    writeSAMCStr(bytes, buffer, (void *)(read.at_line.c_str()+1), read.at_line.size()-1);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt2\n";

    // 2. Write Flag
    setSAMFlag(bytes, buffer, read);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt3\n";

    // 3. Search and Write RNAME
    if (chrs.size() == 0){
        tmp_char = '*';
        writeSAM(bytes, buffer, &tmp_char, 1);
    } else{
       if (is_b){
            chr_idx = getChrsIdx(aligned_sai_b[sai_i], chrs);
        }else{
            chr_idx = getChrsIdx(aligned_sai_f[sai_i], chrs);
        }

        writeSAMCStr(bytes, buffer, (void *)(chrs[chr_idx].name.c_str()+1), chrs[chr_idx].name.length()-2);
    }
    writeSAMTab(bytes, buffer);   
    //cerr<<"chk_pt4\n";

    // 4. Write POS
    if (is_b){
        tmp_val = aligned_sai_b[sai_i] - chrs[chr_idx].begin;
        writeSAMVal(bytes, buffer, &tmp_val, POS_BIT);
    }
    else{
        writeSAMVal(bytes, buffer, &aligned_sai_f[sai_i], POS_BIT);
    }
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt5\n";

    // 5. write QS
    tmp_val = 255;
    writeSAMVal(bytes, buffer, &tmp_val, QS_BIT);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt6\n";
    
    // 6. write CIGAR
    tmp_val = read.seq_len;
    tmp_char = 'M';
    writeSAMVal(bytes, buffer, &tmp_val, MATCH_BIT);
    writeSAM(bytes, buffer, &tmp_char, 1);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt7\n";

    // 7. RNEXT (pair-end)
    tmp_char = '*';
    writeSAM(bytes, buffer, &tmp_char, 1);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt8\n";

    // 8. PNEXT (pair-end)
    tmp_char = '0';
    writeSAM(bytes, buffer, &tmp_char, 1);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt9\n";

    // 9. TLEN (pair-end)
    writeSAM(bytes, buffer, &tmp_char, 1);
    writeSAMTab(bytes, buffer);
    //cerr<<"chk_pt10\n";

    // 10. SEQ
    if (is_b){
        writeSAM(bytes, buffer, &read.seq, read.seq_len);
    }else{
        for (uint32_t i = 0; i < read.seq_len; i++){
            tmp_char = read.seq[read.seq_len-1-i];
            seq_revc[i] = getReverseComplement(tmp_char);          
        }
        writeSAM(bytes, buffer, &seq_revc, read.seq_len);
    }
    writeSAMTab(bytes, buffer);            
    
    // 11. QUAL
    writeSAM(bytes, buffer, (void *)(read.q_score.c_str()), read.seq_len);
    writeSAMTab(bytes, buffer);

    // 12. Optional 
    setSAMOptional(bytes, buffer, read, suppress_aligned);
    
    tmp_char = '\n';
    writeSAM(bytes, buffer, &tmp_char, 1);
}


void convertSAM(FILE *fp_sam, 
                std::vector<read2Bit_t> &reads, 
                uint32_t *sai, 
                char *buffer, 
                std::vector<chr_t> &chrs){

    uint64_t bytes = 0;
    uint32_t aligned_sai_b[ALIGN_REPORT_NUM];
    uint32_t aligned_sai_f[ALIGN_REPORT_NUM];

    uint32_t aligned_num_b = 0;
    uint32_t aligned_num_f = 0;
    uint32_t suppress_aligned = 0;


    for (uint32_t i = 0; i < reads.size(); i++) {

        memset(aligned_sai_b, 0, sizeof(aligned_sai_b));
        memset(aligned_sai_f, 0, sizeof(aligned_sai_f));

        aligned_num_b = 0;
        aligned_num_f = 0;
        suppress_aligned = 0;

        read2Bit_t read = reads[i];
        
        // gather all the aligned pos
        if (read.isaligned_bw){
            for (uint32_t j = 0; j < read.high_bw - read.low_bw && j < ALIGN_REPORT_NUM; j++){
                aligned_sai_b[j] = sai[read.low_bw + j] + 1;
                aligned_num_b++;
            }
            suppress_aligned += (read.high_bw - read.low_bw) > ALIGN_REPORT_NUM ? 
                read.high_bw - read.low_bw - ALIGN_REPORT_NUM: 0;

        }
        //cout<<aligned_sai_b[0]<<" ";

        if (read.isaligned_fw){
            for (uint32_t j = 0; j < read.high_fw - read.low_fw && j < ALIGN_REPORT_NUM; j++){
                aligned_sai_f[j] = sai[read.low_fw + j] + 1;
                aligned_num_f++;
            }                
            suppress_aligned += (read.high_fw - read.low_fw) > ALIGN_REPORT_NUM ? 
                            read.high_fw - read.low_fw - ALIGN_REPORT_NUM: 0;
        }

        if ((bytes + 1024) > BUFF_SIZE) {
            writeFile(fp_sam, buffer, bytes);
            bytes = 0;
        }


        // write to SAM file
        for (uint32_t j = 0; j < aligned_num_b; j++){
            writeSAMFields(bytes, 
                            buffer,
                            chrs,
                            read, 
                            suppress_aligned,
                            aligned_sai_b,
                            aligned_sai_f,
                            true,
                            j);
        
        }

        for (uint32_t j = 0; j < aligned_num_f; j++){
            writeSAMFields(bytes, 
                            buffer,
                            chrs,
                            read,
                            suppress_aligned,
                            aligned_sai_b,
                            aligned_sai_f,
                            false,
                            j);
        
        }
    }
    
    if (bytes > 0) {
        writeFile(fp_sam, buffer, bytes);
    }
}

void writePosDirectly(FILE *fp_di, 
                        std::vector<read2Bit_t> &reads,
                        uint32_t *sai,
                        //uint32_t *sai_rev,
                        std::vector<chr_t> &chrs,
                        char *   buffer){

    uint64_t bytes = 0;

    uint32_t sa_tmp = 0;
    uint32_t tmp_val = 0;
    char tmp_char = '0';
    int chr_idx;

    for (uint32_t i = 0; i < reads.size(); i++) {

        if ((bytes + 1024) > BUFF_SIZE) {
            writeFile(fp_di, buffer, bytes);
            bytes = 0;
        }

        read2Bit_t read = reads[i];

        bool aligned = read.isaligned_bw || read.isaligned_fw;

        if (aligned == true){

            // 1. QNAME
            writeSAMCStr(bytes, buffer, (void *)(read.at_line.c_str()), read.at_line.size());
            tmp_char = '\n';
            writeSAM(bytes, buffer, &tmp_char, 1);

            if (read.isaligned_bw){
                for (uint32_t j = 0; j < read.high_bw - read.low_bw; j++){
                    sa_tmp = sai[read.low_bw + j] + 1;
                    chr_idx = getChrsIdx(sa_tmp, chrs);
                    writeSAMCStr(bytes, buffer, (void *)(chrs[chr_idx].name.c_str()+1), chrs[chr_idx].name.length()-2);
                   
                    tmp_char = ' ';
                    writeSAM(bytes, buffer, &tmp_char, 1); 
                    
                    tmp_val = sa_tmp - chrs[chr_idx].begin;
                    writeSAMVal(bytes, buffer, &tmp_val, POS_BIT);

                    tmp_char = ' ';
                    writeSAM(bytes, buffer, &tmp_char, 1); 
                }
            }else{
                for (uint32_t j = 0; j < read.high_fw - read.low_fw; j++){
                    sa_tmp = sai[read.low_fw + j] + 1;
                    chr_idx = getChrsIdx(sa_tmp, chrs);
                    writeSAMCStr(bytes, buffer, (void *)(chrs[chr_idx].name.c_str()+1), chrs[chr_idx].name.length()-2);

                    tmp_char = ' ';
                    writeSAM(bytes, buffer, &tmp_char, 1); 

                    tmp_val = sa_tmp - chrs[chr_idx].begin;
                    writeSAMVal(bytes, buffer, &tmp_val, POS_BIT);
                    
                    tmp_char = ' ';
                    writeSAM(bytes, buffer, &tmp_char, 1); 
                
                }
            }

            tmp_char = '\n';
            writeSAM(bytes, buffer, &tmp_char, 1);
        
        }
    }
    
    if (bytes > 0) {
        writeFile(fp_di, buffer, bytes);
    }

}
