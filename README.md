# The Oodle1 Compression Scheme #

Oodle1 is a data compression scheme developed by RAD Game Tools, commonly used in various videogames. Note that RAD Game Tools also offers a data compression product, featuring a variety of different compression algorithms which are referred to by various ocean-life monikers -- despite its name and common origin, Oodle1 does not appear to be a particularly close relative of the Oodle product library. An example of an instance where Oodle1 is used, is the game Heroes of Might and Magic V, which launched in 2006.

This document attempts to capture a description of the Oodle1 compression scheme. It is primarily focused on the decompression side of the scheme, however presumably readers could infer how to create a compressor, from the descriptions provided here.

The high-level architecture of Oodle1 is comprised of three distinct functional units, in a layered relationship with one another. In order from the bottom layer to the top, they are:
 - A bitstream parser
 - A symbol-coder
 - An LZ77-style dictionary compressor

Each of these units will be detailed, in the following sections, along with a general description of how the scheme is employed.

As a high-level remark, this document will make repeated reference to "the [0.0, 1.0) interval", and "the fixed-point representation of 1.0". Readers unfamiliar with the basic premises of arithmetic coders would likely be benefited by performing some amount of research into them, as the Oodle1 symbol decoder is quite similar to an arithmetic coder, and this document will not explain those concepts. To readers familiar with arithmetic coders, the significance of the [0.0, 1.0) interval and fixed-point representation of real numbers will be clear.

Additionally, while this document will generally reference the 1.0 value as such, the fixed-point representation that Oodle1 has chosen for 1.0, is 0x4000. All references to 0.0 imply the integer value 0, and all references to 1.0 imply the integer value 0x4000. At no point in the Oodle1 compression scheme are floating-point values or operations used; all values and operations are integer, lying in the range [0, 0x4000]. Nevertheless, for the purposes of remaining focused on the fundamental concepts of Oodle1, rather than its concrete implementation, most references will be to the 0.0 and 1.0 values.


## Bitstream Layer ##

The bitstream layer serves to ingest compressed data, and provide it to the symbol-coding layer, in the form of a fixed-point number lying in the interval [0.0, 1.0).

The major operations of the bitstream layer are:
 - Initialize
 - Ingest
 - Peek
 - Consume
 - Get

The bitstream layer preserves a small amount of state, as it works its way through the compressed data:
 - I: A pointer to the next byte to be consumed
 - R: A 32-bit shift register, containing the most recent bytes read
 - M: A 32-bit "shift register modulus", which tracks the value of "full-scale", relative to the data that has been ingested into the shift register
 - L: A 1-bit Least Significant Bit flag

During operation, data is shifted into the shift register in a split "7+1" form; each byte of input data has its seven most significant bits shifted into the least significant bits of the shift register, and the input byte's own least significant bit is held in the LSB flag. When the *next* byte of data is shifted into the register, first the saved LSB is shifted into the LSB of the shift register, before the above 7+1 split is performed again.

In this way, at any point in time, the least significant bit of data that has been read from the compressed data stream is held out-of-line, and is not considered to be part of the shift register, for the purposes of determining its value, but it will be pushed into the shift register at its appropriate position, when the bitstream reader next ingests a byte.


### Initialize ###

The `Initialize` operation primes the bitstream reader, by shifting in the first byte of data, respecting the 7+1 split described above. In pseudocode:


```
def BSInitialize(input):
    I = input
    R = (*I >> 1)
    L = (*I & 0x01)
    M = 0x80
    I++
```


### Ingest ###

The `Ingest` operation shifts bytes of input data into the shift register, until at least 24 bits of data are present in the shift register (that is, until the shift-register modulus is greater than 0x800000). Its operation is in large part a retread of the ground covered above, with respect to how data is shifted into the shift register:

```
def BSIngest():
    while M <= 0x800000:
        R = (R << 1) | L
        R = (R << 7) | (*I >> 1)
        L = (*I & 0x01)
        M <<= 8
        I++
```

### Peek ###

The `Peek' operation allows callers to examine the value that is present in the shift register, without necessarily modifying its contents. This operation requires a parameter from its caller: A specification of the fixed-point representation of "1.0" (f). With this parameter in hand, the operation is as follows:


```
def BSPeek(f):
    BSIngest()
    s = (M / f)
    z = min((R / s), f - 1)
    return z
```

The local variable `s` is a scale factor that projects the fixed-point representation of 1.0, onto the full-range value of the shift register (i.e., the shift register modulus).

The local variable `z` is the projection of the shift register's current contents, onto the interval [0.0, 1.0).

Although the concept of projecting the input bits into the interval [0.0, 1.0) is fundamental to arithmetic coders, this form of projection-through-division is one of the atypical aspects of the Oodle1 Symbol-Coder; an ordinary arithmetic coder performs its rescaling quite differently.

Note that integer (truncating) division is used.


### Consume ###

The `Consume` operation updates the contents of the shift register, to reflect the consumption of a previously-peeked and decoded symbol:


```
def BSConsume(MZ, SPN, f):
    s = (M / f)
    sz = MZ * s
    R -= sz
    if MZ < (f - SPN):
        M = SPN * s
    else:
        M -= sz
```

The parameter `MZ` is best described as the "lower bound of z" -- the lowest value of `z` that would have resulted in decoding the same symbol as the previously-peeked `z` actually resulted in (this statement may become more clear after reading the description of the Symbol-Coding layer). In the terms of arithmetic coder, this is the cumulative frequency of all symbols preceding the decoded symbol.

The parameter `SPN` is the "span of z", which is the fixed-point span that `z` occupied in the interval [0.0, 1.0); in the terms of an arithmetic coder, this would be the frequency of `z`.

The parameter `f` is once again the fixed-point representation of 1.0.

Once again, the local variable `s` is a scalar for projecting values between the range present in the shift register, and the fixed-point interval.

The local vairable `sz` is the result of scaling `MZ` back up into the range of the shift register.

The modification of `R` and `M` combine "consumption" of the bits of `z` from the shift register, with "rescaling" of the remaining bits in `R` (by reducing `M`).

Note that the update of `M` is split, depending on whether the symbol just decoded was the last symbol in the effective alphabet (the one whose upper bound approaches 1.0), however the conceptual effect is the same:
 - In the case where a non-last symbol has been decoded, the upper bound on the significance of whatever remains in `R` is `SPN` (scaled by `s`); if `R` were more significant than that, then the next symbol in the alphabet would have been decoded, instead.
 - In the case where the last symbol has been decoded, then the upper bound of remaining significance cannot be known as concretely, since `BSPeek` clamped the value of `z` to fit the caller's interval, so no argument can be made about "would have decoded a different symbol". Thus, the (scaled) lower bound of `z` is subtracted from `M`. This may lead to suboptimal compression, but does not harm correctness, as long as both compressor and decompressor agree on this treatment.


## Get ##

The `Get` operation is effectively a `Peek` operation joined with a `Consume` operation.


```
def BSGet(f):
    BSIngest()
    s = (M / f)
    z = min((R / s), f - 1)
    sz = z * s
    R -= sz
    if z < (f - 1):
        M = s
    else:
        M -= sz
```

The update of `M` differs slightly from the `Consume` operation, however the fundamental logic is the same: If the number decoded from the shift register lies within the [0.0, 1.0) interval, then there cannot have been more than `scale` significance left, otherwise we would have decoded a number beyond the interval. If we did decode a number that could lie beyond the interval (evidenced by having approached the 1.0 representation), then we cannot know for certain how much significance is left; we can only subtract the significance that we have removed. This approaches the upper bound for remaining significance, but is not precise.


## Symbol-Coding Layer ##

The Symbol-Coding layer is responsible for taking in fixed-point numbers from the bitstream layer, and providing decoded symbols to the LZ layer. Fundamentally, each symbol decoder is an adaptive, fixed-point decoder that operates along the same basic principles as a classical arithmetic coder, but which deviates from the traditional formulation of an arithmetic coder in various details.

The major operations of a symbol coder are:
 - Initialize
 - Decay
 - Renormalize
 - Decode

The motivations of the `Initialize` and `Decode` operations are obvious. The `Decay` and `Renormalize` operations are components of the coder's adaptation scheme; the coder learns symbols and their relative occurence rates, as data is coded, and it adjusts the "weights" of each symbol accordingly. With the term "weight", we are speaking of the proportion of the [0.0, 1.0) interval that the symbol occupies. This is related to the frequency of the symbol, however is not strictly equivalent to its frequency, due to the function of the adaptation scheme.

The 'Decay' operation is responsible for reducing the weights of infrequently used symbols over time, and eventually aging them out of the interval entirely (their weight / span becomes 0, and thus cannot be decoded without being re-learned).

The `Renormalize` operation is responsible for keeping the total weight of all learned symbols within some manageable bound that guarantees that there will be sufficient resolution within the bitstream layer, to unambiguously decode symbols. As its name implies, this largely consists of just multiplying the weight of each symbol by some appropriate scalar value to bring the sum of all weights back within the desired bound.

As an ancillary function, `Renormalize` also serves the role of placing recently-learned symbols into the active alphabet. New symbols are not immediately placed into the alphabet; this only occurs at renormalization. In the interim, the symbols are held in a separate "probationary" alphabet, within which they can be decoded, but using a coding that is not yet weighted by their occurrence rate.

Early after initialization, the coder performs renormalization operations frequently -- it can be expected to be rapidly learning many new symbols. The renormalization interval decays exponentially, until it reaches a steady-state value that is set during coder initialization.


### Initialize ###

The `Initialize` operation prepares the coder for use:

```
def SCInitialize(p_AS, p_US):
    US = p_US
    fill(SW[], 0x4000)
    LS[0] = 0
    SW[0] = 0
    LSW[0] = 4
    TLW = 4
    HLS = 0
    HLSN = 0
    NRW = 8
    DT = clamp((p_AS - 1) * 32, 256, 15160)
    RRI = 4
    RI = clamp((p_AS - 1) * 2, 128, (DT / 2) - 32)
```

The `p_AS` parameter is the absolute size of the alphabet being decoded. This is the number of unique symbols that could be decoded, without regard for how many are expected to occur.

The `p_US` parameter is the number of unique symbols that are expected to occur, in the course of decompression. It is stored for future use, during symbol learning.

The `LS` member variable is an array of the values of each symbol in the active alphabet. The value of the "0" symbol is set to 0, however it is crucial to understand that the "0" symbol is special in the Oodle1 symbol-coder (as will become apparent when we examine the `Decode` operation); the "0" symbol is *not* the symbol used to represent the byte value 0, despite this initialization. In fact, the "0" symbol can never be the final result of a `Decode` operation, and this initialization serves only as an anchor to document the existence of the `LS` array.

The `SW` member variable is an array of *cumulative* weights for each symbol in the active alphabet. These will be used to decode `z` values obtained from the bitstream layer into symbols. In the terms of an arithmetic coder, these are the cumulative frequencies of each symbol, setting thresholds along the [0.0, 1.0) interval where each symbol's span begins. The value 0x4000 is the fixed-point representation of 1.0 that is used by the Oodle1 symbol coder, and the entire array of symbol weights is initialized with this value, save for the first element, which naturally begins at 0.0.

The `LSW` member variable is an array of "recent occurrence frequency" which is maintained for each symbol in the alphabet (active and probationary), which tracks how often each symbol is observed, and is used during renormalization to apportion a suitable span of the [0.0, 1.0) interval. The frequency of the "0" symbol is set to 4.

The `TLW` member variable is the sum of all `LSW`, so it is also initialized to 4.

The `HLS` member variable is the value of the highest symbol learned thus far. It is, naturally, 0, as only the "0" symbol is in the active alphabet, and the probationary alphabet is empty. Note that `HLS` is equivalent to "number of symbols learned, minus one", as it is used in this context in various places.

The `HLSN` member variable is the value of the highest symbol learned, as of the most recent renormalization.

The `NRW` member variable is the "next renormalization weight" -- once this much weight has been accumulated by learned symbols (`TLW`), the coder will be renormalized.

The `DT` member variable is the "decay threshold" -- once this much weight has been accumulated by learned symbols, renormalization will be preceded by a `Decay` operation.

The `RRI` member variable is the "rapid renormalization interval" -- this is the amount by which `NRW` will be incremented, during the early phases of decoding.

The `RI` member variable is the "renormalization interval" -- this is the amount by which `NRW` will be incremented, once the "rapid learning" phase is over, and the coder reaches steady-state operation.

Implementers must note that the implementation presented in this document requires that the arrays `LS`, `LSW`, and `SW` be oversized by at least 2 elements, compared to the actual alphabet size. This is because slots must be reserved for both the "0" symbol (at 0.0), and the "1" symbol (at 1.0). The "1" can never be decoded, however it is required to be present, for the `Decode` operation to operate properly, as presented.


### Decay ###

As previously described, the `Decay` operation serves to decay the weighting of symbols that have not been used recently, potentially aging them out of the active alphabet entirely.

```
def SCDecay():
    LSW[0] /= 2
    TLW = SLW[0]
    hsw = 0
    hsi = 0
    for i in [1, HLS]:
        while LSW[i] <= 1:
            if i < HLS:                 # Condition 1
                LSW[i] = LSW[HLS]
                LSW[HLS] = 0
                LS[i] = LS[HLS]
                HLS -= 1
            else:                       # Condition 2
                LSW[i] = 0
                HLS -= 1
                break
        LSW[i] /= 2
        TLW += LSW[i]
        if LSW[i] > hsw:                # Condition 3
            hsw = LSW[i]
            hsi = i
    if hsw && (hsi != HLS):             # Condition 4
        swap(LSW[HLS], LSW[hsi])
        swap(LS[HLS], LS[hsi])
    if (HLS != US) && (LSW[0] == 0):    # Condition 5
        LSW[0] = 1
        TLW += 1
```

The `hsw` local variable stores the weight of the highest-weighted symbol in the active alphabet. This is discovered through the course of the weight-decay operation, at "Condition 3".

The `hsi` local variable stores the index of the highest-weighted symbol in the active alphabet. Discovered alongside `hsw`.

The `Decay` operation consists of four distinct major operations:

First, symbols are aged out of the active alphabet. This occurs when a symbol's weight has reached 1. In this event, the highest learned symbol is moved to the to the place of the symbol, effectively deleting it from the alphabet (and shrinking the alphabet by 1); this occurs at "Condition 1". If the symbol is, itself, the highest learned symbol ("Condition 2", implying that the alphabet will be exhausted by deleting the symbol), then the symbol is deleted without replacement, and the scan through the alphabet is terminated.

Second, the symbols that remain in the active alphabet each have their weight halved (with `TLW` being updated accordingly). At "Condition 3", the highest-weighted symbol is discovered, as the alphabet is iterated through.

At "Condition 4", the highest-weighted symbol is moved to the end of the active alphabet, if it is not already there.

Finally, at "Condition 5", it is ensured that the "0" symbol does not get aged out of the alphabet, unless the active alphabet already contains all possible symbols -- the reason why it is acceptable to age the "0" symbol out in this case will become clear, when we describe the `Decode` operation.


### Renormalize ###

The 'Renormalize' operation has also already had its high-level motivation described above; it serves to bound the accumulated weights of the various symbols, and to apportion spans of the [0.0, 1.0) interval to each symbol that has been learned so far, thus placing them into the active alphabet.

```
def SCRenormalize():
    q = 0x20000 / TLW
    SW[0] = 0
    aw = (LSW[0] * q) / 8
    for i in [1, HLS]:
        SW[i] = aw
        aw += (LSW[i] * q) / 8
    RRI *= 2
    NRW = TLW + min(RRI, RI)
    HLSN = HLS
    fill(SW[i..], 0x4000)
```

The `q` local variable is a quantum that is used for rescaling the accumulated weights into the interval [0, 0x4000). To mitigate the effects of integer truncation, the quantum is not derived directly from 0x4000, but is instead derived from 0x20000, with a final divide-by-8 operation being performed when updating each individual weight. This preserves a greater degree of accuracy, when renormalizing.

The `aw` local variable is an acummulator, tracking the amount of weight that has been distributed to symbols in the active alphabet, so far.

The core loop of this function is quite straightforward: It assigns each symbol a lower bound within the [0.0, 1.0) interval, which is the accumulated weight of each preceding symbol, and it calculates the span of that symbol, by rescaling its accumulated weight into the interval.

Once the active alphabet has been updated, `RRI` is updated, to achieve the exponential decay of the rapid renormalizations that are performed shortly after coder startup, and the threshold for the next renormalization is set (`NRW`).

The "highest symbol at last renormalization" (`HLSN`) member variable is updated, with the number of symbols in the active alphabet; this is required by the `Decode` operation, to determine how many symbols exist within the probationary alphabet.

Finally, the weights of symbols lying outside the active alphabet are set to the 1.0 value (thus apportioning them no span in the interval).


### Decode ###

At last, we can describe the `Decode` operation. This operation is provided with the number of unique symbols that might be decoded in the current context of the LZ layer, and ingests a fixed-point number from the bitstream layer, to form into a symbol. Along the way, it learns new symbols, and monitors the occurrence rates of old.

```
def SCDecode(sc):
    if TLW >= NRW:              # Condition 1
        if TLW >= DT:           # Condition 2
            SCDecay()
        SCRenormalize()
    z = BSPeek(0x4000)
    for i in [0,HLSN]:          # Decode From Active
        if (SW[i] <= z) && (SW[i + 1] > z):
            break
    BSConsume(SW[i], SW[i + 1] - SW[i], 0x4000)
    LSW[i] += 1
    TLW += 1
    if i != 0:                  # Active Symbol
        return LS[i]
    else:                       # 0 Symbol
        if HLS != HLSN:         # Condition 3
            b = BSGet(2)
            if b:               # Probationary Symbol
                i = BSGet(HLS - HLSN) + HLSN + 1
                LSW[i] += 2
                TLW += 2
                return LS[i]
        HLS += 1
        LS[HLS] = BSGet(sc)
        LSW[HLS] += 2
        TLW += 2
        if HLS == US:           # Condition 4
            TLW -= LSW[0]
            LSW[0] = 0
        return LS[HLS]
```

The first step of the `Decode` operation is to check whether renormalization (Condition 1) or decay (Condition 2) are required. If they are, those operations are performed.

Next, the local variable `z` is obtained from the bitstream layer; this is a fixed-point number lying in the interval [0.0, 1.0).

The active alphabet is consulted ("Decode From Active"), and it is determined which portion of the active alphabet `z` lies within. This results in a tentative symbol `i`.

At this point, the bitstream layer is informed that the symbol has been consumed (with knowledge about its exact probability span), and the learned occurrence rate of the symbol (`LSW[i]`) is incremented, along with the total occurrence rate (`TLW`).

Now, a decision is made about whether the decoded symbol is a terminal symbol (i.e., not the "0" symbol); if it is ("Active Symbol"), then the symbol is returned to the LZ layer.

If the "0" symbol was decoded ("0 Symbol"), this is an indicator to the coder that the symbol being decoded is either:
 - A brand-new symbol, not yet learned, or
 - A symbol learned, but in the probationary alphabet, since renormalization has not occurred since learning

These cases are distingushed through the combination of "Condition 3" and "Probationary Symbol"; if no new symbols have been learned since the last renormalization, then the symbol cannot be a member of the probationary alphabet, as the probationary alphabet is empty (all known symbols are in the active alphabet). If the probationary alphabet is not empty, then the next bit of the fixed-point number in the bitstream layer is consulted.

If that bit is a 1, then the symbol being decoded is in the probationary alphabet ("Probationary Symbol"). All symbols in the probationary alphabet are assumed to have equal probability, so the bitstream layer is asked to decode a number bounded by the size of the probationary alphabet (`i`). That symbol is then adjusted to reflect its position near/at the end of the alphabet, its occurrence rate is adjusted (`LSW[i]`, and `TLW`), and it is returned to the caller.

If the probationary alphabet is empty, or the next bit decoded was a 0, then the symbol being decoded is brand new. The number of learned symbols is incremented, the new symbol is decoded from the bitstream layer (using knowledge provided by the caller, about how many distinct symbols can occur in the current LZ context), and its weights are updated (`LSW[HLS]` and `TLW`).

Before returning the newly learned symbol to the caller, a check is performed at "Condition 4", to see if the entire set of unique symbols has now been learned into the active and probationary alphabets. If all possible symbols reside within the union of those alphabets, then the "0" symbol should no longer occur -- all symbols have been learned, so the alphabets should not expand further. Thus, the learned weight of the "0" symbol is set to 0 (with `TLW` being modified accordingly). Note that this does not immediately impact the ability to decode "0" symbols -- this is still necessary, as long as the probationary alphabet still has symbols in it. However, at the next renormalization, all members of the probationary alphabet will get moved into the active alphabet, and (assuming no other members of the active alphabet age out) the "0" symbol will be aged out.

Note that symbols that are either newly-learned, or in the probationary alphabet, have their occurrence rate incremented by 2, rather than the 1 that is used for members of the active alphabet. This serves two purposes:
 - It allows new symbols to rapidly establish themselves in the interval, if they occur frequently (e.g., the tone of the data being decompressed has significantly shifted)
 - It ensures that newly-learned symbols are not immediately aged out, if they have the misfortune to be learned immediately prior to a renormalization

Also note that the occurrence rate of the "0" symbol is tracked, just like the members of the active alphabet -- if many new symbols are being learned, then the "0" symbol will accordingly be given a greater share of the interval, to give it shorter encodings. Conversely, if new symbols are only being learned rarely, the "0" symbol will shrink in the interval, to give shorter encodings to the symbols of the active alphabet.


## LZ Layer ##

The LZ layer is, in some ways, a quite classic implementation of LZ77. The decompressor reads symbols from the symbol-coding layer, and constructs the symbols that it reads into either:
 - A "repeat" code, which specifies a length & offset of already-decompressed data to replay into the output stream, or
 - A "literal" code, which provides a raw byte value to be emitted into the output stream

The major operations of the LZ layer are:
 - Initialize
 - Decode

The major interfaces of the LZ layer are:
 - Decode symbols, via the Symbol-Coder layer
 - Emit decompressed data bytes

As an LZ-style dictionary decompressor, the LZ layer has a concept of a "window", which bounds the amount of previously-decompressed data that can be referenced by "repeat" codes. The maximum operational size of the Oodle1 LZ window is configurable through its header, however there is a hard cap at 256KiB -- the window cannot be larger than this.

When describing the `Decode` operation, we will additionally call on the concept of an "effective" window size, which is the operational window size, but not greater than the number of bytes already decompressed -- this value bounds the maximum possible offset, and is intrinsic to how repeat-offset symbols are decoded.

A point of distinction between Oodle1's LZ scheme, as compared with a more typical LZ77 scheme, comes in how it encodes the offsets of repeat codes. The offset is not a single integer number; instead, it is formed from three distinct fields, which (presented in the order in which they must be decoded) are:
 - A "one-byte" field ('1b')
 - A "one-k" field ('4b')
 - A "four-byte" field ('1k')
The total offset is formed using the expected mathematical operation: `(1k * 1024) + (4b * 4) + 1b`

As a more major deviation from "classical" LZ77, which uses a single set of Huffman codes for decoding symbols in all contexts, Oodle1's LZ layer is quite "context-dependent"; it does not get its symbols from a single Symbol-Coder, and instead has as many as 327 Symbol-Coders at its disposal, to decode symbols from various contexts:

Four (4) decoders are used for decoding literals, selected on the basis of the number of bytes thus far decompressed, modulo 4. This scheme can be expected to provide excellent results when dealing with 8-bit RGBA image data, as each decoder will see the values of only a single color component (Red, Green, Blue, or Alpha), which can reasonably be expected to change fairly slowly. This design likely also lends itself to 32-bit floating-point values, where the exponent bits tend to take on a fairly confined set of values, in typical data.

Sixty-five (65) decoders are used for decoding repeat lengths. The decoder to use is selected by the length of the *previous* decoded repeat length-code (which is not precisely the same thing as the previously decoded repeat length, as we will discuss soon).

One (1) decoder is used for decoding the "one-byte" field of repeat offsets.

As many as two hundred and fifty-six (256) decoders are used for decoding the "four-byte" field of repeat offsets. The actual number of used decoders is bounded by the length of the decompressed data -- the decoder to use is selected by the "one-k" field of the repeat offset. This has the effect of allowing these coders to adapt to varying access patterns to data in different parts of the file.

One (1) decoder is used for decoding the "one-k" field of repeat offsets.

As part of invoking decoders to decode symbols, the LZ layer must specify the *effective* size of the "alphabet" that the decoder is operating with -- the number of distinct codes that it might be able to decode, given the current context. Some decoders have an effective alphabet size that is fixed (either in the abstract, or at initialization time, based on content in the Oodle1 header), but other decoders have a dynamic effective alphabet size, which changes as data is decompressed:
 - The 4 literal decoders have their effective alphabet size set through the Oodle1 header
 - The 65 repeat-length decoders have a fixed (effective) alphabet size, of 65
 - The 1 "one-byte" repeat-offset decoder has an effective alphabet size that is set at initialization time, from the operational window size
 - The 256 "four-byte" repeat-offset decoders have an effective alphabet size that is dynamic at runtime (more details in a moment)
 - The 1 "one-k" repeat-offset decoder has an effective alphabet size that is dynamic at runtime


### Initialize ###

The `Initialize` operation consumes the Oodle1 header, and many of the parameters that inform its actions will be described in more detail in the section on the header.

```
def LZInitialize(header):
    WS = header[0] >> 9
    LAS = header[0] & 0x1ff
    ulc = header[1] & 0x1ff
    l1ko = header[1] >> 19

    foreach litDecoder:
        litDecoder.Initialize(LAS, ulc)

    rl = [ (header[2] >> 24) & 0xff, (header[2] >> 16) & 0xff, (header[2] >> 8) & 0xff, header[2] & 0xff ]
    for i in [0,3]:
        for j in [0,15]:
            lenDecoders[(i * 16) + j].Initialize(65, rl[i])
    lenDecoders[64].Initialize(65, rl[3])

    O1AS = min(4, windowSize + 1)
    offsetCoder1.Initialize(O1AS, O1AS)

    o4as = min(256, (windowSize / 4) + 1)
    foreach offsetCoder4:
        offsetCoder4.Initialize(o4as, o4as)

    o1024as = (windowSize / 4) + 1
    offsetCoder1024.Initialize(o1024as, l1ko + 1)
```

The `WS` member variable is the operational window size of the LZ decompressor.

The `LAS` member variable is the absolute size of the literal alphabet.

The `ulc` local variable is the number of unique symbols that may be decoded, from the literal alphabet.

The `l1ko` local variable is the largest "one-k" offset value that will be used while decompressing.

The `O1AS` member variable is the absolute size of the "one-byte" offset alphabet.



### Decode ###

The first step of the `Decode` operation is to decode a symbol, using the repeat-length decoder indexed by the previous repeat length-code. The decoded symbol becomes the new "previous repeat length-code", and is then assessed:

If the repeat length-code is 0, then a literal is to be decompressed. The `Decode` operation decodes a symbol, using the literal decoder indexed by the current length of decompressed data (modulo 4). That symbol is emitted as a decompressed byte. At this point, the `Decode` operation ends. The caller may invoke it again, if more data is to be decompressed.

If the repeat length-code is not 0, then it is interpreted, according to this table:

 Code | Repeat Length
------+---------------
    1 | 2
  ... | ...
   60 | 61
   61 | 128
   62 | 192
   63 | 256
   64 | 512

Then, the repeat-offset is decoded: First, a symbol is decoded, using the "one-byte" decoder; the "one-byte" field is equal to this symbol, plus one (i.e., decoding a "2" symbol means that the "one-byte" field is 3).

Next, a symbol is decoded, using the "one-k" decoder. The effective alphabet size of this decoder is determined on an operation-by-operation basis, by the effective window size -- namely, the effective size of the alphabet is `(effectiveWindowSize / 1024) + 1`. The "one-k" field is equal to this symbol.

Finally, a symbol is decoded, using the "four-byte" decoder indexed by the "one-k" field. The effective alphabet size of this decoder is also determined on an operation-by-operation basis, by the effective window size -- namely, the effective size of the alphabet is `min(256, (effectiveWindowSize / 4) + 1)`. The "four-byte" field is equal to this symbol.

At this point, the complete repeat-offset can be constructed, and, with both the offset and length known, the data can be replayed from the buffer of already-decompressed data. At this point, the `Decode` operation ends, and may be invoked again.

It is worth noting that, while the dynamic alphabet sizes of the "four-byte" and "one-k" decoders somewhat bound the decodable offsets, relative to the effective window size, they do not actually ensure that the total offset value is within the effective window (or even within the operational window). It is possible (but ill-formed) for the compressor to form repeat codes that refer to data lying outside the window, with indeterminate results.

In psedo-code, the function of the `Decode` operation is as follows:

```
def LZDecode():
    RLC = lenCoders[RLC].Decode(65)
    if RLC == 0:
        lit = litCoders[OLEN & 0x03].Decode(LAS)
        LZEmitLiteral(lit)
    else:
        rl = lenTable[RLC]
        ews = min(WS, OLEN)
        o1b = offsetCoder1.Decode(O1AS) + 1
        o1k = offsetCoder1024.Decode((ews / 1024) + 1)
        o4b = offsetCoder4[o1k].Decode(min(256, (ews / 4) + 1))
        o = (o1k * 1024) + (o4b * 4) + o1
        LZEmitRepeat(rl, o)
```

The `RLC` member variable is the previous repeat length-code.

The `OLEN` member variable is the length of the output emitted so far.

The `LAS` member variable is the absolute size of the literal alphabet.

The `rl` local variable is the actual length of the repeat; `lenTable` is the previously-described table for interpreting length codes.

The `ews` local variable is the effective window size; `WS` is the operational window size.

The `O1AS` member variable is the absolute size of the "one-byte" offset alphabet.

The `o1b`, `o4b`, and `o1k` local variables are the separate components of the repeat offset; `o` is the final offset value.

The `LZEmitLiteral` pseudo-operation appends a single byte to the stream of decompressed data.

The `LZEmitRepeat` pseudo-operation replays a string of bytes from the already-decompressed data, into the output stream.



## The Oodle1 Header ##

The Oodle1 header is 12 bytes of metadata that must be provided alongside the data to be decompressed (although it is not required that it be prepended directly onto it); this metadata consists of the parameters that the compressor used, when compressing the data. We present the data here in the order that it would be seen on a little-endian platform; if the compressor was run on a big-endian platform, it will be necessary to endian-correct each four-byte word of the Oodle1 header.

```
     3                   2                   1
   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  windowSize                 | litAlphabetSize |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     largest1KOffset     |    Reserved       |  uniqLitCount   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  | uniqRepLens0  | uniqRepLens1  | uniqRepLens2  | uniqRepLens3  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

`litAlphabetSize` specifies the absolute size of the literal alphabet. Often this is 256 (meaning that all 8-bit bytes can be decompressed), however this is not required.

`windowSize` specifies the operational window size for the LZ layer. This, in turn, impacts the absolute alphabet sizes for repeat-offset codes. In pathological cases, where the window is less than four bytes in size, the size of the "one-byte" alphabet is the window size, plus one. Generally, however, the window will be larger, and the "one-byte" alphabet is exactly four (the symbols for 0 through 3). The size of the "four-byte" alphabet is `min(256, (windowSize / 4) + 1)`. The size of the "one-k" alphabet is `(windowSize / 1024) + 1`.

`uniqLitCount` specifies the number of unique literal values that occur within the data. This can be less than the absolute alphabet size.

`largest1KOffset` specifies the largest "one-k" repeat offset code that occurs within the data.

`uniqRepLens0` specifies the number of unique repeat length-codes that occur, in the context of the "0 through 15" length decoders (i.e., the decoders used when the previous repeat had a length-code in the range 0 to 15, inclusive).

`uniqRepLens1` specifies the number of unique repeat length-codes that occur, in the context of the "16 through 23" length decoders.

`uniqRepLens2` specifies the number of unique repeat length-codes that occur, in the context of the "24 through 31" length decoders.

`uniqRepLens3` specifies the number of unique repeat length-codes that occur, in the context of the "32 through 64" length decoders.


Note that a number of the parameters described above influenced both the alphabet sizes of various Symbol-Coders -- a distinction exists between the *absolute* size of a decoder's alphabet, and the *effective* size of its alphabet, as described in the section on Symbol-Coders.

Note also that a number of the parameters described the number of unique symbols that occurred in various contexts. These impact the operation of the corresponding Symbol-Coders, as described in that section.


## Decompressing Data ##

Interfacing with the LZ layer to decompress Oodle1 data is almost a trivial operation: Initialize each of the functional units, pad the input data with 0 bytes, until its length is an even multiple of 4, and invoke the LZ layer's `Decode` operation, until the expected amount of data has been decompressed. Note that it is an expectation of the scheme, that the total uncompressed size of the data will be communicated to the decompressor, out-of-band; there is no EOF symbol.


## Oodle1 in Granny2 Files ##

A common user of Oodle1 compression, is Granny2 data files, which are used by various games for storing assets. This document does not generally concern itself with the structure or interpretation of Granny2 files, as our focus is on the Oodle1 compression scheme, however we will give a very brief treatment to a couple of details about the usage of Oodle1, within Granny2 files:

First, the blocks of compressed data in Granny2 files do not necessarily contain a single stream of compressed data. Instead, each compressed block may contain up to three distinct streams of compressed data, concatenated together. The results of decompressing those streams are likewise concatenated together, to form the complete decompressed block. The Oodle1 scheme requires that the decompressor be aware of how many bytes of data it is expected to decompress, and accordingly Granny2 files specify the uncompressed length that marks the end of each data stream. After decompressing enough data to reach the switchover point from one stream to the next, the Granny2 decoder must initialize a new Oodle1 decompressor with the next stream's header, and continue decompressing.

Second, as there are potentially three Oodle1 streams per Granny2 compressed block, each compressed block begins with three Oodle1 headers (12 bytes apiece, 36 bytes in total).


## Reference Implementation ##

Included alongside this document is a basic implementation of an Oodle1 decoder. It forgoes any attempt at optimization, in the interest of being compact and readable.
