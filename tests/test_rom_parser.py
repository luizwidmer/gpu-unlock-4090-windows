import struct
import unittest
from check_rom import inspect_rom

def rom(device=0x2684):
    data = bytearray(512)
    data[:2] = b'\x55\xaa'
    struct.pack_into('<H', data, 24, 32)
    data[32:36] = b'PCIR'
    struct.pack_into('<HH', data, 36, 0x10de, device)
    struct.pack_into('<H', data, 42, 24)
    struct.pack_into('<H', data, 48, 1)
    data[53] = 128
    return data

class RomTests(unittest.TestCase):
    def test_valid_is_inventory_only(self):
        result = inspect_rom(rom())
        self.assertTrue(result['contains_4090_id'])
        self.assertFalse(result['hardware_execution_enabled'])
        self.assertFalse(result['live_prom_mapping_verified'])

    def test_wrong_gpu_not_accepted(self):
        self.assertFalse(inspect_rom(rom(0x2704))['contains_4090_id'])

    def test_truncated(self):
        for length in (0, 1, 25, 511):
            with self.assertRaises(ValueError):
                inspect_rom(rom()[:length])

    def test_bad_pointer(self):
        data = rom()
        struct.pack_into('<H', data, 24, 510)
        with self.assertRaises(ValueError):
            inspect_rom(data)

    def test_bad_extent(self):
        for blocks in (0, 2, 65535):
            data = rom()
            struct.pack_into('<H', data, 48, blocks)
            with self.assertRaises(ValueError):
                inspect_rom(data)

    def test_unterminated_chain(self):
        data = rom()
        data[53] = 0
        with self.assertRaises(ValueError):
            inspect_rom(data)

    def test_bad_signature(self):
        data = rom()
        data[32:36] = b'FAKE'
        with self.assertRaises(ValueError):
            inspect_rom(data)

    def test_nvgi_prefix_is_file_only(self):
        data = bytearray(512) + rom()
        data[:4] = b'NVGI'
        result = inspect_rom(data)
        self.assertEqual(result['file_view_offset'], 512)
        self.assertFalse(result['live_prom_mapping_verified'])

    def test_npde_continues_past_pci_last_flag(self):
        first = rom()
        first[64:68] = b'NPDE'
        struct.pack_into('<HHH', first, 68, 0x101, 12, 1)
        first[74] = 0
        result = inspect_rom(first + rom())
        self.assertEqual(len(result['images']), 2)

    def test_nvgi_without_valid_image_rejected(self):
        with self.assertRaises(ValueError):
            inspect_rom(b'NVGI' + b'\0'*1020)

    def test_truncated_npde_rejected(self):
        data = rom()
        data[64:68] = b'NPDE'
        struct.pack_into('<HHH', data, 68, 0x101, 600, 1)
        with self.assertRaises(ValueError):
            inspect_rom(data)
