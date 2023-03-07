#! /usr/bin/env python3

import os
import struct
import sys

import numpy as np
import scipy.io

for wavfilename in sys.argv[1:]:

    base, ext = os.path.splitext(wavfilename)
    dirname, basename = os.path.split(base)

    if ext.lower() != ".wav":
        print(f"fatal: expected only .wav file arguments.  got: {wavfilename}", file=sys.stderr)
        sys.exit(1)

    rate, data = scipy.io.wavfile.read(wavfilename)

    # convert unsigned bytes to signed bytes (avoiding overflows)
    tmp = data.astype(np.int32)
    tmp = tmp - 128
    data = tmp.astype(np.int8)

    print(f"{rate=} {len(data)=} {data.dtype=}")

    samfilename = os.path.join(f"{dirname}",f"{basename}.sam")

    with open(samfilename,"wb") as f:

        code_from_mame_source_for_reference = """
            osd_fread(f,buf,4);
            if (memcmp(buf, "MAME", 4) == 0)
            {
                osd_fread(f,&smplen,4);         /* all datas are LITTLE ENDIAN */
                osd_fread(f,&smpfrq,4);
                smplen = intelLong (smplen);  /* so convert them in the right endian-ness */
                smpfrq = intelLong (smpfrq);
                osd_fread(f,&smpres,1);
                osd_fread(f,&smpvol,1);
                osd_fread(f,&dummy,2);
                if ((smplen != 0) && (samples->sample[i] = malloc(sizeof(struct GameSample) + (smplen)*sizeof(char))) != 0)
                {
                   samples->sample[i]->length = smplen;
                   samples->sample[i]->volume = smpvol;
                   samples->sample[i]->smpfreq = smpfrq;
                   samples->sample[i]->resolution = smpres;
                   osd_fread(f,samples->sample[i]->data,smplen);
                }
            }
        """

        f.write("MAME".encode('utf-8'))       # magic "MAME"
        f.write(struct.pack('<I',len(data)))  # sample length
        f.write(struct.pack('<I',rate     ))  # sample freq
        f.write(struct.pack('<B',8        ))  # resolution (bits?)
        f.write(struct.pack('<B',255      ))  # volume
        f.write(struct.pack('<H',0        ))  # dummy
        f.write(data)                         # data
