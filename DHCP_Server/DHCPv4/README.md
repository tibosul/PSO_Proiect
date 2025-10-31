# DHCPv4 Server Implementation

Server DHCPv4 funcțional pentru Linux, compatibil cu formatul ISC DHCP.

## 📁 Structura Proiectului

```
DHCPv4/
├── src/                    # Fișiere sursă (.c)
│   ├── config_v4.c        # Parsare configurație
│   ├── lease_v4.c         # Management lease-uri
│   └── ip_pool.c          # Pool-uri de adrese IP
│
├── include/               # Header files (.h)
│   ├── config_v4.h
│   ├── lease_v4.h
│   ├── ip_pool.h
│   ├── string_utils.h
│   ├── network_utils.h
│   ├── time_utils.h
│   └── encoding_utils.h
│
├── utils/                 # Funcții utilitare generale
│   ├── string_utils.c     # Operații pe string-uri (trim)
│   ├── network_utils.c    # Parsare IP/MAC, formatare
│   ├── time_utils.c       # Parsare/formatare ISC DHCP time
│   └── encoding_utils.c   # Octal/hex encoding pentru client ID
│
├── tests/                 # Teste unitare
│   ├── test_config_v4.c
│   ├── test_lease_extended.c
│   ├── test_lease_save.c
│   └── test_ip_pool.c
│
├── config/                # Fișiere de configurare
│   └── dhcpv4.conf        # Configurație server (ISC DHCP format)
│
├── data/                  # Date persistente
│   └── dhcpv4.leases      # Bază de date lease-uri
│
├── docs/                  # Documentație
│
├── build/                 # Artifacts de build (generat automat)
│   ├── *.o               # Fișiere obiect
│   └── test_*            # Executabile test
│
├── Makefile              # Build system
└── README.md             # Acest fișier
```

## 🚀 Compilare și Rulare

### Compilare toate testele:
```bash
make
```

### Compilare test specific:
```bash
make test_config_v4
make test_lease_extended
make test_lease_save
make test_ip_pool
```

### Rulare teste:
```bash
# Rulează toate testele
make test

# Sau individual:
./build/test_config_v4 config/dhcpv4.conf
./build/test_lease_extended data/dhcpv4.leases
./build/test_lease_save
./build/test_ip_pool
```

### Curățare:
```bash
make clean
```

### Ajutor:
```bash
make help
```

## 📦 Componente Implementate

### ✅ Configuration Management (`config_v4.c/h`)
- Parsare fișier de configurare format ISC DHCP
- Support pentru:
  - Opțiuni globale (DNS, lease time, etc.)
  - Multiple subnets cu range-uri
  - Host reservations (IP-uri statice)
  - Domain names și opțiuni per subnet

### ✅ Lease Management (`lease_v4.c/h`)
- Format compatibil ISC DHCP
- State machine complet:
  - `FREE`, `ACTIVE`, `EXPIRED`, `RELEASED`, `ABANDONED`, `RESERVED`, `BACKUP`
- Timestamps complete:
  - `starts`, `ends`, `tstp`, `cltt`
- Client identification:
  - MAC address
  - Client ID (option 61)
  - Hostname
  - Vendor class identifier (option 60)
- Persistență în fișier
- Load și save complet

### ✅ IP Pool Management (`ip_pool.c/h`)
- Alocare dinamică de adrese IP
- Support pentru rezervări statice
- Tracking stări IP:
  - `AVAILABLE`, `ALLOCATED`, `RESERVED`, `EXCLUDED`, `CONFLICT`
- Căutare IP liber eficient

### ✅ Utility Functions (`utils/`)
Funcții generale reutilizabile:

#### String Utils (`string_utils.c/h`)
- `trim()` - Eliminare whitespace de la începutul și sfârșitul string-urilor

#### Network Utils (`network_utils.c/h`)
- `parse_ip_address()` - Parsare adrese IPv4
- `parse_mac_address()` - Parsare adrese MAC
- `format_mac_address()` - Formatare MAC la string
- `parse_ip_list()` - Parsare listă de IP-uri separate prin virgulă

#### Time Utils (`time_utils.c/h`)
- `parse_lease_time()` - Parsare format ISC DHCP ("4 2024/10/26 14:30:00")
- `format_lease_time()` - Formatare Unix timestamp la ISC DHCP format

#### Encoding Utils (`encoding_utils.c/h`)
- `parse_client_id_from_string()` - Parsare octal/hex escape sequences
- `format_client_id_to_string()` - Formatare client ID cu octal escapes

## 🧪 Teste

Toate testele sunt în directorul `tests/`:

1. **test_config_v4.c** - Test parsare configurație
2. **test_lease_extended.c** - Test funcționalități lease complete
3. **test_lease_save.c** - Test salvare și formatare lease
4. **test_ip_pool.c** - Test alocare IP pool

## 📝 Format Fișiere

### dhcpv4.conf
Format compatibil ISC DHCP:
```
authoritative;
default-lease-time 7200;
max-lease-time 86400;

subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    option domain-name "corporate.example.com";
}
```

### dhcpv4.leases
Format ISC DHCP leases:
```
lease 192.168.1.100 {
    starts 4 2024/10/26 14:30:00;
    ends 4 2024/10/26 22:30:00;
    binding state active;
    hardware ethernet 00:11:22:33:44:aa;
    uid "\001\000\021\042\063\104\252";
    client-hostname "laptop-john";
}
```

## 🛠️ Dezvoltare

### Adăugare fișier sursă nou:
1. Adaugă fișierul `.c` în `src/` (sau `utils/` pentru funcții generale)
2. Adaugă header-ul `.h` în `include/`
3. Actualizează `SOURCES` (sau `UTILS_SOURCES`) în Makefile

### Adăugare funcție utilitar:
1. Creează fișierul în `utils/` (exemplu: `my_utils.c`)
2. Creează header-ul în `include/` (`my_utils.h`)
3. Adaugă la `UTILS_SOURCES` în Makefile
4. Include header-ul în fișierele care îl folosesc

### Adăugare test nou:
1. Creează `tests/test_<nume>.c`
2. Adaugă regula de build în Makefile

## 🔧 Compilator și Flag-uri

- **Compiler:** GCC
- **Standard:** C11
- **Flags:** `-Wall -Wextra -g`
- **Include path:** `-Iinclude` (automat adăugat)

## 📊 Statistici Cod

- **Fișiere sursă:** 3 (config_v4.c, lease_v4.c, ip_pool.c)
- **Fișiere utils:** 4 (string_utils.c, network_utils.c, time_utils.c, encoding_utils.c)
- **Fișiere header:** 7
- **Teste:** 4
- **Linii cod:** ~1500 LOC (fără teste și utils)

## ⚠️ Limitări Actuale

- ❌ DHCP packet handling (DISCOVER, OFFER, REQUEST, ACK) - **NU IMPLEMENTAT**
- ❌ Socket UDP binding pe port 67
- ❌ Network interface management
- ❌ Thread safety

## 🎯 Next Steps

1. **CRITICAL:** Implementare DHCP protocol handler
2. Socket UDP management
3. Packet parsing și building
4. Integration testing
5. Multi-threading pentru multiple requests

## 📖 Referințe

- RFC 2131 - DHCP Protocol
- RFC 2132 - DHCP Options
- ISC DHCP Documentation
- dhcpd.conf(5) manual page
- dhcpd.leases(5) manual page
