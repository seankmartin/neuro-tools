#include "AxonaBinReader.h"
#include "Utils.h"
#include <fstream>
#include <iostream>
#include <array>
#include <chrono>
#include <vector>

int16_t AxonaBinReader::ConvertBytes(char b1, char b2)
{
    uint16_t us = (uint8_t)b1 * 256 + (uint8_t)b2;
    int16_t out = (int16_t)us;
    return out;
}

AxonaBinReader::AxonaBinReader()
{
}

AxonaBinReader::AxonaBinReader(std::string name)
{
    Init(name);
}

int *AxonaBinReader::ParseReferences()
{
    std::ifstream set_file(_set_fname);
    std::string line;
    std::string ref_match = "b_in_ch_";
    std::string base_match = "ref_";
    int refs[64] = { 0 };
    int base_refs[8] = { 0 };
    while (std::getline(set_file, line))
    {
        std::cout << line << std::endl;
        std::size_t found = line.rfind(ref_match, 0);
        if (found != std::string::npos)
        {
            found = ref_match.length();
            std::string end_bit = line.substr(
                found, line.length());
            std::size_t end_find = end_bit.rfind(" ");
            int chan = std::stoi(end_bit.substr(0, end_find));
            int ref = std::stoi(
                end_bit.substr(end_find + 1, end_bit.length()));
            refs[chan] = ref;
        }
        else
        {
            found = line.rfind(base_match, 0);
            if (found != std::string::npos)
            {
                found = base_match.length();
                std::string end_bit = line.substr(
                    found, line.length());
                std::size_t end_find = end_bit.rfind(" ");
                int ref_idx = std::stoi(end_bit.substr(0, end_find));
                int ref_chan = std::stoi(
                    end_bit.substr(end_find + 1, end_bit.length()));
                base_refs[ref_idx] = ref_chan;
            }
        }
    }
    // system("pause");
    for (int i = 0; i < 64; ++i)
    {
        int ch = refs[i];
        if (ch > 7)
        {
            std::cout << "Error! Reference channel out of range -" << ch << std::endl;
            exit(-1);
        }
        refs[i] = base_refs[ch];
        std::cout << i << ", " << ch << ", " << refs[i] << std::endl;
    }
    return refs;
}

void AxonaBinReader::Init(std::string name)
{
    SetSetFname(name);
    std::string base_name = name.substr(0, name.length() - 4);
    std::string bin_name = base_name;
    std::string inp_name = base_name;
    bin_name.append(".bin");
    SetBinFname(bin_name);
    base_name.append("_shuff.bin");
    _out_fname = base_name;
    inp_name.append(".inp");
    _out_inpname = inp_name;
    _dir_name = dir_from_file(_out_fname);
}

bool const AxonaBinReader::ToInp()
{
    long long fsize = GetFileSize(GetBinFname());
    long long total_samples = fsize / _chunksize;
    const int buff_size = _chunksize;

    std::vector<char> buffer(buff_size, 0);
    std::vector<uint64_t> digital_vals;

    std::ifstream infile;
    infile.open(_bin_fname, std::ios::binary | std::ios::in);
    uint32_t sample_count = 0;

    uint16_t last_input_val = 1000;
    uint16_t last_output_val = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    while (infile.read(buffer.data(), buffer.size()))
    {
        uint16_t input_val = (256 * (uint8_t)buffer[9]) + (uint8_t)buffer[8];
        uint16_t output_val = (256 * (uint8_t)buffer[417]) + (uint8_t)buffer[416];
        uint32_t timestamp = sample_count;
        if (input_val != last_input_val)
        {
            char c = 'I';
            digital_vals.push_back(
                ((uint64_t)timestamp * 16777216) + (65536 * (uint64_t)c) + (uint64_t)input_val);
            last_input_val = input_val;
        }
        if (output_val != last_output_val)
        {
            char c = 'O';
            digital_vals.push_back(
                ((uint64_t)timestamp * 16777216) + (65536 * (uint64_t)c) + (uint64_t)output_val);
            last_output_val = output_val;
        }
        sample_count += 1;
    }
    infile.close();
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    std::cout << "Elapsed time to read: " << elapsed.count() << " s\n";
    std::cout << "Number of input output samples: " << digital_vals.size() << std::endl;

    start = std::chrono::high_resolution_clock::now();
    std::ofstream outfile(_out_inpname, std::ios::out | std::ios::binary);
    outfile << std::string("bytes_per_sample ") << 7 << std::endl;
    outfile << std::string("timebase ") << 16000 << std::endl;
    outfile << std::string("num_inp_samples ") << digital_vals.size() << std::endl;
    outfile << std::string("data_start");
    for (int i = 0; i < digital_vals.size(); ++i)
    {
        auto byte_arr = IntToBytes(digital_vals[i]);
        outfile.write(byte_arr.data(), 7);
    }
    outfile << std::string("data_end");
    outfile.close();
    finish = std::chrono::high_resolution_clock::now();
    elapsed = finish - start;
    std::cout << "Elapsed time to write: " << elapsed.count() << " s\n";
    std::cout << "Result is at: " << _out_inpname << std::endl;
    return true;
}

bool const AxonaBinReader::Read()
{
    long long fsize = GetFileSize(GetBinFname());
    long long total_samples = fsize / _chunksize;
    total_samples *= _samples_per_chunk;
    std::cout << "Total samples " << total_samples << std::endl;

    // Set up buffers and storage vectors
    const int buff_size = _chunksize;
    const long long tp_chunk_size = _transpose_chunk_size;
    std::vector<char> buffer(buff_size, 0);
    long long row_dim;
    long long col_dim;
    if (_transpose)
    {
        row_dim = tp_chunk_size;
        col_dim = _num_channels;
    }
    else
    {
        row_dim = _num_channels;
        col_dim = total_samples;
    }
    std::vector<std::vector<int16_t>> channel_data(
        row_dim, std::vector<int16_t>(col_dim, 0));
    std::vector<uint64_t> digital_vals;

    // Open the file
    std::ifstream infile;
    infile.open(_bin_fname, std::ios::binary | std::ios::in);
    long long sample_count = 0;

    // Setup the header and start the clock
    auto start = std::chrono::high_resolution_clock::now();
    std::ofstream outfile;
    //const char header[4] = "bax";
    //outfile.write(header, 3);
    //std::string str = std::to_string(total_samples);
    //while (str.length() != 10)
    //{
    //    str.insert(0, "0");
    //}
    //char const *pchar = str.c_str();
    //outfile.write(pchar, 10);
    //outfile.write(header, 3);

    // Setup variables
    uint16_t last_input_val = 1000;
    uint16_t last_output_val = 1000;

    // Do all the file writing at the end
    long long sample_size_to_write;
    long long iterate_over;

    // Read info
    while (infile.read(buffer.data(), buffer.size()))
    {
        // Inp file calculation
        uint16_t input_val = (256 * (uint8_t)buffer[9]) + (uint8_t)buffer[8];
        uint16_t output_val = (256 * (uint8_t)buffer[417]) + (uint8_t)buffer[416];
        uint32_t timestamp = sample_count / _samples_per_chunk;

        // Only record when the data changes
        if (input_val != last_input_val)
        {
            char c = 'I';
            digital_vals.push_back(
                ((uint64_t)timestamp * 16777216) + (65536 * (uint64_t)c) + (uint64_t)input_val);
            last_input_val = input_val;
        }
        if (output_val != last_output_val)
        {
            char c = 'O';
            digital_vals.push_back(
                ((uint64_t)timestamp * 16777216) + (65536 * (uint64_t)c) + (uint64_t)output_val);
            last_output_val = output_val;
        }

        // Channel sample calculation
        for (int i = _header_bytes; i < _chunksize - _trailer_bytes; i = i + _sample_bytes)
        {
            int compare_val = (i - _header_bytes) / 2;

            long long row_sample = compare_val % _num_channels;
            long long col_sample = sample_count + (compare_val / _num_channels);
            row_sample = _reverse_map_channels[row_sample];
            if (_transpose)
            {
                long long temp = col_sample % tp_chunk_size;
                col_sample = row_sample;
                row_sample = temp;
            }
            int16_t val = ConvertBytes(buffer[i + 1], buffer[i]);
            channel_data[row_sample][col_sample] = val;
        }

        sample_count += _samples_per_chunk;

        if (_transpose && ((sample_count % tp_chunk_size == 0) || (sample_count == total_samples)))
        {   
            // TODO account for the last chunk
            sample_size_to_write = _sample_bytes * (long long)_num_channels;
            iterate_over = sample_count % tp_chunk_size;
            if (sample_count != total_samples)
                iterate_over = tp_chunk_size;
            else
            {
                iterate_over = sample_count % tp_chunk_size;
                if (iterate_over == 0)
                    iterate_over = tp_chunk_size;
            }
            int iters_left = (total_samples - sample_count) / tp_chunk_size + (sample_count != total_samples);
            std::cout << "Writing " << iterate_over << " samples this loop" << std::endl;
            std::cout << iters_left << " iterations left" << std::endl;

            outfile.open(_out_fname, std::ios::out | std::ios::binary | std::ios::app);
            // Write the channel data out
            for (long long i = 0; i < iterate_over; ++i)
            {
                outfile.write((char*)channel_data[i].data(), sample_size_to_write);
            }

            // Write the channel data out in blocks
            if (_do_split)
            {
                std::vector<int16_t> temp_holder;
                temp_holder.reserve(iterate_over * _chans_per_tetrode);
                for (int i = 0; i < _num_channels; ++i)
                {
                    bool at_tetrode_start = (i % 4 == 0);
                    if (at_tetrode_start)
                    {
                        int tetrode = i / 4;
                        std::string temp_fname = _dir_name;
                        temp_fname.append(_out_split_dir);
                        temp_fname.append("\\");
                        std::string mod_str = std::to_string(tetrode);
                        temp_fname.append(mod_str);
                        temp_fname.append("\\recording.dat");
                        std::cout << "Writing split data to " << temp_fname << std::endl;
                        outfile.close();
                        outfile.open(temp_fname, std::ios::out | std::ios::binary | std::ios::app);
                    }
                    if (at_tetrode_start)
                    {
                        for (long long j = 0; j < iterate_over; ++j)
                        {
                            for (int k = 0; k < _chans_per_tetrode; ++k)
                            {
                                temp_holder.push_back(channel_data[j][i + k]);
                            }
                        }
                        outfile.write((char*)temp_holder.data(), (long long)(_sample_bytes) * iterate_over * _chans_per_tetrode);
                        temp_holder.clear();
                        }
                    }
                }
            }
            outfile.close();
    }
    infile.close();
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    std::cout << "Elapsed time to read channels and write data: " << elapsed.count() << " s\n";
    start = std::chrono::high_resolution_clock::now();

    if (!_transpose)
    {
        sample_size_to_write = _sample_bytes * total_samples;
        iterate_over = _num_channels;

        // Write the channel data out
        outfile.open(_out_fname, std::ios::out | std::ios::binary);
        for (long long i = 0; i < iterate_over; ++i)
        {
            outfile.write((char*)channel_data[i].data(), sample_size_to_write);
        }

        // Write the channel data out in blocks
        if (_do_split)
        {
            std::vector<int16_t> temp_holder;
            if (_split_tp)
            {
                temp_holder.reserve(total_samples * _chans_per_tetrode);
            }
            for (int i = 0; i < _num_channels; ++i)
            {
                int mod_bit = i % 4;
                if (mod_bit == 0)
                {
                    int tetrode = i / 4;
                    std::string temp_fname = _dir_name;
                    temp_fname.append(_out_split_dir);
                    temp_fname.append("\\");
                    std::string mod_str = std::to_string(tetrode);
                    temp_fname.append(mod_str);
                    temp_fname.append("\\recording.dat");
                    std::cout << "Writing split data to " << temp_fname << std::endl;
                    outfile.close();
                    outfile.open(temp_fname, std::ios::out | std::ios::binary);
                }
                if (_split_tp)
                {
                    if (i % 4 == 0)
                    {
                        for (long long j = 0; j < total_samples; ++j)
                        {
                            for (int k = 0; k < _chans_per_tetrode; ++k)
                            {
                                temp_holder.push_back(channel_data[i + k][j]);
                            }
                        }
                        outfile.write((char*)temp_holder.data(), (long long)(_sample_bytes)*total_samples * _chans_per_tetrode);
                        temp_holder.clear();
                    }
                }
                else
                {
                    if (i % 4 < _chans_per_tetrode)
                    {
                        outfile.write((char*)channel_data[i].data(), sample_size_to_write);
                    }
                }
            }
        }
    }
    outfile.close();

    std::cout << "Number of input output samples: " << digital_vals.size() << std::endl;

    std::ofstream out_inp(_out_inpname, std::ios::out | std::ios::binary);
    out_inp << std::string("bytes_per_sample ") << 7 << std::endl;
    out_inp << std::string("timebase ") << 16000 << std::endl;
    out_inp << std::string("num_inp_samples ") << digital_vals.size() << std::endl;
    out_inp << std::string("data_start");
    for (int i = 0; i < digital_vals.size(); ++i)
    {
        auto byte_arr = IntToBytes(digital_vals[i]);
        out_inp.write(byte_arr.data(), 7);
    }
    out_inp << std::string("data_end");
    out_inp.close();
    finish = std::chrono::high_resolution_clock::now();
    elapsed = finish - start;
    std::cout << "Elapsed time to write: " << elapsed.count() << " s\n";
    std::cout << "Channel data is at: " << _out_fname << std::endl;
    std::cout << "Input data is at: " << _out_inpname << std::endl;

    return true;
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        std::cout << "Please enter as AxonaBinary setfile_location chans_per_tet tranpose(T/F) do_split(T/F) [tranpose_split(T/F) out_split_loc]" << std::endl;
        exit(-1);
    }
    std::string location(argv[1]);
    if (!file_exists(location))
    {
        std::cout << location << " is not a valid file path" << std::endl;
        exit(-1);
    }
    AxonaBinReader axbr{location};
    std::cout << "Converting " << location << std::endl;
    axbr.SetChansPerTet(std::stoi(argv[2]));
    if (argv[3] == std::string("T"))
    {
        std::cout << "Will transpose the main outfile" << std::endl;
        axbr.SetTranspose(true);
    }
    if (argv[4] == std::string("T"))
    {
        std::cout << "Will split the output files" << std::endl;
        axbr.SetDoSplit(true);
        if (argv[5] == std::string("T"))
        {
            std::cout << "Will transpose the split outfiles" << std::endl;
            axbr.SetSplitTranspose(true);
        }
        axbr.SetSplitDir(argv[6]);
    }
    axbr.Read();
}