/* Very dumb CRC-32 implementation */
UINT32 Crc32(
  CONST VOID *Buffer,
  UINTN Length
  )
{
	CONST UINT8 *Buf8 = Buffer;
	UINT32 Remainder = 0xffffffff;

	for (; Length; Length--) {
		int i;

		Remainder = Remainder ^ *Buf8;
		for (i = 0; i < 8; i++)
			Remainder = (Remainder >> 1) ^ ((Remainder & 1) * 0xEDB88320);

		Buf8++;
	}

	return Remainder ^ 0xffffffff;
}