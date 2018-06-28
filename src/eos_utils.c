#include "eos_utils.h"
#include "os.h"


unsigned char const BASE58ALPHABET[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};


unsigned char buffer_to_encoded_base58(unsigned char *in, unsigned char length,
                         unsigned char *out,
                         unsigned char maxoutlen) {
    unsigned char tmp[164];
    unsigned char buffer[164];
    unsigned char j;
    unsigned char startAt;
    unsigned char zeroCount = 0;
    if (length > sizeof(tmp)) {
        THROW(INVALID_PARAMETER);
    }
    os_memmove(tmp, in, length);
    while ((zeroCount < length) && (tmp[zeroCount] == 0)) {
        ++zeroCount;
    }
    j = 2 * length;
    startAt = zeroCount;
    while (startAt < length) {
        unsigned short remainder = 0;
        unsigned char divLoop;
        for (divLoop = startAt; divLoop < length; divLoop++) {
            unsigned short digit256 = (unsigned short)(tmp[divLoop] & 0xff);
            unsigned short tmpDiv = remainder * 256 + digit256;
            tmp[divLoop] = (unsigned char)(tmpDiv / 58);
            remainder = (tmpDiv % 58);
        }
        if (tmp[startAt] == 0) {
            ++startAt;
        }
        buffer[--j] = (unsigned char)BASE58ALPHABET[remainder];
    }
    while ((j < (2 * length)) && (buffer[j] == BASE58ALPHABET[0])) {
        ++j;
    }
    while (zeroCount-- > 0) {
        buffer[--j] = BASE58ALPHABET[0];
    }
    length = 2 * length - j;
    if (maxoutlen < length) {
        THROW(EXCEPTION_OVERFLOW);
    }
    os_memmove(out, (buffer + j), length);
    return length;
}

const unsigned char hex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

void array_hexstr(char *strbuf, const void *bin, unsigned int len) {
    while (len--) {
        *strbuf++ = hex_digits[((*((char *)bin)) >> 4) & 0xF];
        *strbuf++ = hex_digits[(*((char *)bin)) & 0xF];
        bin = (const void *)((unsigned int)bin + 1);
    }
    *strbuf = 0; // EOS
}

char* i64toa(int64_t i, char b[]) {
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int64_t shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

/**
 * Decodes tag according to ASN1 standard.
*/
static void decodeTag(uint8_t byte, uint8_t *cls, uint8_t *type, uint8_t *nr) {
    *cls = byte & 0xc0;
    *type = byte & 0x20;
    *nr = byte & 0x1f;
}

/**
 * Decoder supports only 'OctetString' from DER encoding.
 * To get more information about DER encoding go to
 * http://luca.ntop.org/Teaching/Appunti/asn1.html.
*/

#define NUMBER_OCTET_STRING 0x04

/**
 * tlv buffer is 5 bytes long. First byte is used for tag.
 * Next, up to four bytes could be used to to encode length.
*/
bool tlvTryDecode(uint8_t *buffer, uint32_t bufferLength, uint32_t *fieldLenght, bool *valid) {
    uint8_t class, type, number;
    decodeTag(*buffer, &class, &type, &number);
    
    if (number != NUMBER_OCTET_STRING) {
        *valid = false;
        return false;
    }

    if (bufferLength < 2) {
        *valid = true;
        return false;
    }
    bufferLength--;
    buffer++;
    // Read length
    uint32_t length;
    uint8_t byte = *buffer;
    if (byte & 0x80) {
        uint8_t i;
        uint8_t count = byte & 0x7f;
        if (count > 4) {
            // Its allowed to have up to 4 bytes for length
            // [.] Tag
            //    [. . . .] Length
            *valid = false;
            return false;
        }
        
        if (count > bufferLength) {
            *valid = true;
            return false;
        }
        
        length = 0;
        for (i = 0; i < count; ++i) {
            length = (length << 8) | *(buffer + i);
        }
    } else {
        length = byte;
    }
    *fieldLenght = length;
    *valid = true;

    return true;
}