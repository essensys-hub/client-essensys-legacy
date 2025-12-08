# Communications du Client Embarqué

## Vue d'ensemble

Le client embarqué BP_MQX_ETH utilise plusieurs protocoles de communication pour interagir avec différents périphériques et systèmes :

- **I2C** : Communication avec les boîtiers auxiliaires (BA)
- **SPI** : Accès aux EEPROM pour l'adresse MAC et la clé serveur
- **UART** : Réception des données TeleInfo (compteur Linky) et debug
- **Ethernet** : Communication TCP/IP avec le serveur via la stack RTCS

Chaque protocole est configuré et géré par des modules dédiés dans le firmware MQX.

---

## I2C - Communication avec les Boîtiers Auxiliaires

### Description

Le bus I2C est utilisé pour communiquer avec les boîtiers auxiliaires (BA) qui contrôlent les équipements de la maison (lumières, prises, etc.). Le boîtier principal (BP) agit comme maître I2C et interroge périodiquement les BA esclaves.

### Configuration I2C

```c
// Configuration du bus I2C
#define I2C_DEVICE_POLLED "i2c0:"
#define I2C_BUS_ADDRESS_BP 0x50        // Adresse du boîtier principal
#define I2C_BUS_ADDRESS_BA 0x60        // Adresse de base des BA

// Vitesse du bus
uint_32 baud_rate = 50000;  // 50 kHz
ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_BAUD, &baud_rate);

// Configuration en mode maître
ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_MASTER_MODE, NULL);

// Configuration de l'adresse station
uint_32 station_addr = I2C_BUS_ADDRESS_BP;
ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_STATION_ADDRESS, &station_addr);
```

### Protocole I2C

Le protocole de communication avec les BA suit cette séquence :

1. **Écriture des données** : Le BP envoie une commande au BA
2. **Repeated Start** : Génération d'un restart I2C
3. **Lecture de la réponse** : Le BP lit 5 octets de réponse du BA

#### Format de la trame émise

```
[Code_Trame][Données...][CRC_LSB][CRC_MSB]
```

- **Code_Trame** : Identifiant du type de commande
- **Données** : Payload de la commande
- **CRC** : Checksum CRC-16 sur toute la trame

#### Format de la réponse

```
[Code_Trame][CRC_Envoyé_LSB][CRC_Envoyé_MSB][CRC_Réponse_LSB][CRC_Réponse_MSB]
```

- **Code_Trame** : Doit correspondre au code émis
- **CRC_Envoyé** : Le BA renvoie le CRC qu'il a reçu (vérification)
- **CRC_Réponse** : CRC calculé sur les 3 premiers octets

### Exemple de code I2C

```c
// Fonction d'écriture/lecture I2C avec un BA
signed long sl_fct_write_polled(
    MQX_FILE_PTR fd,              // File descriptor I2C
    unsigned char uc_BANumber,    // Numéro du BA (0-15)
    unsigned char *p_uc_buffer,   // Buffer de données à envoyer
    unsigned short us_n,          // Nombre d'octets
    unsigned char *p_uc_Answer    // Réponse du BA
)
{
    signed long result;
    unsigned long dest_addr;
    
    // Protection mutex pour environnement multitâche
    _lwsem_wait(&lock_I2C);
    
    // Configuration de l'adresse destination
    dest_addr = I2C_BUS_ADDRESS_BA + uc_BANumber;
    result = ioctl(fd, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &dest_addr);
    
    // Émission du START et de l'adresse
    fwrite(buffer, 1, 0, fd);  // START condition
    result = ioctl(fd, IO_IOCTL_FLUSH_OUTPUT, &param);
    
    // Écriture des données
    fwrite(p_uc_buffer, 1, us_n, fd);
    fflush(fd);  // Attendre fin d'émission
    
    // Repeated START pour lecture
    ioctl(fd, IO_IOCTL_I2C_REPEATED_START, NULL);
    
    // Configuration de la lecture (5 octets)
    uint_32 rx_request = 5;
    ioctl(fd, IO_IOCTL_I2C_SET_RX_REQUEST, &rx_request);
    
    // Lecture de la réponse
    fread(p_uc_buffer, 1, 5, fd);
    
    // Vérification du CRC
    // ...
    
    // STOP condition
    ioctl(fd, IO_IOCTL_I2C_STOP, NULL);
    
    // Libération du mutex
    _lwsem_post(&lock_I2C);
    
    // Tempo pour laisser le BA traiter
    _time_delay(100);  // 100 ms
    
    return result;
}
```

### Gestion des erreurs I2C

Le système détecte et gère plusieurs types d'erreurs :

#### Types d'erreurs

- **Erreur d'adresse** : Le BA ne répond pas au START
- **Erreur d'émission** : Échec d'envoi des données
- **Erreur de CRC** : Checksum invalide dans la réponse
- **Erreur de code** : Le code de trame retourné ne correspond pas
- **Blocage I2C** : Le bus I2C se bloque (détecté par timeout)

#### Récupération sur blocage

En cas de blocage détecté, le système effectue une réinitialisation complète du bus I2C :

```c
if(uc_FlagReinitialiserI2C != 0)
{
    // Fermeture du driver I2C
    fclose(fd_BA_I2C);
    _time_delay(10);
    
    // Réouverture
    fd_BA_I2C = fopen(I2C_DEVICE_POLLED, NULL);
    
    // Reconfiguration complète
    ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_BAUD, &baud_rate);
    ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_MASTER_MODE, NULL);
    ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_STATION_ADDRESS, &station_addr);
}
```

Le système tente jusqu'à `uc_NB_REINIT_MAX` réinitialisations avant d'abandonner.

### Timeouts I2C

- **Timeout de communication** : 100 ms entre chaque transaction
- **Tempo inter-trame** : 100 ms après chaque échange pour laisser le BA traiter
- **Timeout de réinitialisation** : 10 ms entre chaque tentative

---

## SPI - Accès aux EEPROM

### Description

Le bus SPI est utilisé pour accéder à deux EEPROM externes :

1. **EEPROM Adresse MAC** (CS0) : Stocke l'adresse MAC Ethernet du boîtier
2. **EEPROM Soft** (CS2) : Stocke la clé de cryptage serveur et d'autres paramètres

### Configuration SPI

```c
// Ouverture du port SPI
MQX_FILE_PTR fd_SPI_EEPROM;
fd_SPI_EEPROM = fopen("spi0:", NULL);

// Configuration en mode maître
uint_32 param = SPI_DEVICE_MASTER_MODE;
ioctl(fd_SPI_EEPROM, IO_IOCTL_SPI_SET_TRANSFER_MODE, &param);

// Configuration du mode clock (CPOL=0, CPHA=0)
param = SPI_CLK_POL_PHA_MODE0;
ioctl(fd_SPI_EEPROM, IO_IOCTL_SPI_SET_MODE, &param);

// Configuration Big Endian (MSB first)
param = SPI_DEVICE_BIG_ENDIAN;
ioctl(fd_SPI_EEPROM, IO_IOCTL_SPI_SET_ENDIAN, &param);

// Configuration de la vitesse (500 kHz)
param = 500000;
ioctl(fd_SPI_EEPROM, IO_IOCTL_SPI_SET_BAUD, &param);
```

### Sélection du Chip Select

```c
// Sélection EEPROM Adresse MAC (CS0)
void vd_SpiSelectEEPROMAdresseMac(void)
{
    uint_32 cs = MCF5XXX_QSPI_QDR_QSPI_CS0;
    ioctl(fd_SPI_EEPROM, IO_IOCTL_SPI_SET_CS, &cs);
}

// Sélection EEPROM Soft (CS2)
void vd_SpiSelectEEPROMSoft(void)
{
    uint_32 cs = MCF5XXX_QSPI_QDR_QSPI_CS2;
    ioctl(fd_SPI_EEPROM, IO_IOCTL_SPI_SET_CS, &cs);
}
```

### Utilisation typique

```c
// Ouverture du SPI (une seule fois au démarrage)
vd_SpiOpen();

// Lecture de l'adresse MAC
vd_SpiSelectEEPROMAdresseMac();
// ... opérations de lecture SPI ...

// Lecture de la clé serveur
vd_SpiSelectEEPROMSoft();
// ... opérations de lecture SPI ...
```

### Paramètres SPI

- **Vitesse** : 500 kHz
- **Mode** : Mode 0 (CPOL=0, CPHA=0)
- **Ordre des bits** : MSB first (Big Endian)
- **Chip Select** : CS0 pour MAC, CS2 pour clé serveur

---

## UART - TeleInfo et Debug

### Description

Le système utilise deux liaisons UART :

1. **UART TeleInfo** : Réception des données du compteur Linky (ERDF)
2. **UART Debug** : Sortie de debug pour le développement

### Configuration UART TeleInfo

```c
// Ouverture du port série TeleInfo
#define TELEINFO_DEVICE_INTERRUPT "ttya:"
#define TELEINFO_BAUD_RATE 1200

MQX_FILE_PTR fd_teleinfo;
fd_teleinfo = fopen(TELEINFO_DEVICE_INTERRUPT, IO_SERIAL_NON_BLOCKING);

// Configuration du baud rate (1200 bps)
uint_32 baud = TELEINFO_BAUD_RATE;
ioctl(fd_teleinfo, IO_IOCTL_SERIAL_SET_BAUD, &baud);

// Configuration de la parité (paire)
uint_32 parity = IO_SERIAL_PARITY_EVEN;
ioctl(fd_teleinfo, IO_IOCTL_SERIAL_SET_PARITY, &parity);

// Configuration 7 bits de données
uint_32 data_bits = 7;
ioctl(fd_teleinfo, IO_IOCTL_SERIAL_SET_DATA_BITS, &data_bits);

// Activation de la réception
boolean disable_rx = FALSE;
ioctl(fd_teleinfo, IO_IOCTL_SERIAL_DISABLE_RX, &disable_rx);
```

### Format des trames TeleInfo

Le compteur Linky envoie des trames au format suivant :

```
<STX>
<LF> ETIQUETTE <SP> DONNEE <SP> CHECKSUM <CR>
<LF> ETIQUETTE <SP> DONNEE <SP> CHECKSUM <CR>
...
<ETX>
```

#### Caractères de contrôle

- **STX** (0x02) : Début de trame
- **ETX** (0x03) : Fin de trame
- **LF** (0x0A) : Début de groupe d'information
- **CR** (0x0D) : Fin de groupe d'information
- **SP** (0x20) : Séparateur
- **EOT** (0x04) : Pause émission (le compteur va réémettre)

#### Calcul du checksum

```c
// Calcul du checksum TeleInfo
unsigned char uc_fct_Calcul_CheckSum_Tele(
    unsigned char *puc_buffer, 
    uint_16 us_Nb_Octets
)
{
    uint_16 i;
    unsigned char checksum = 0;
    
    // Somme de tous les caractères
    for(i = 0; i < us_Nb_Octets; i++)
    {
        checksum += puc_buffer[i];
    }
    
    // Garder seulement les 6 bits de poids faible
    checksum &= 0x3F;
    
    // Ajouter 0x20 pour obtenir un caractère imprimable
    checksum += 0x20;
    
    return checksum;
}
```

### Parsing des trames TeleInfo

```c
// Lecture non-bloquante du port série
void vd_RSTeleinfoRead(MQX_FILE_PTR fd)
{
    unsigned char caractere;
    
    // Lecture du buffer de réception MQX
    while(read(fd, &caractere, 1) != 0 && 
          us_TeleinfoNbOctetsRecus < us_TAILLE_BUFFER_RX_TELEINFO)
    {
        // Filtrage des caractères de contrôle
        if(caractere != 0x02 &&  // STX
           caractere != 0x03 &&  // ETX
           caractere != 0x04)    // EOT
        {
            uc_TeleinfoBufferRx[us_TeleinfoNbOctetsRecus] = caractere;
            us_TeleinfoNbOctetsRecus++;
        }
        
        // Si pause émission, vider le buffer
        if(caractere == 0x04)
        {
            us_TeleinfoNbOctetsRecus = 0;
        }
    }
}
```

### Étiquettes TeleInfo supportées

| Étiquette | Description | Exemple |
|-----------|-------------|---------|
| ADCO | Adresse du compteur | 031228232024 |
| OPTARIF | Option tarifaire | HC.. (Heures Creuses) |
| PTEC | Période tarifaire en cours | HP.. (Heures Pleines) |
| ISOUSC | Intensité souscrite | 60 (ampères) |
| HCHC | Index Heures Creuses | 003309966 (Wh) |
| HCHP | Index Heures Pleines | 007772110 (Wh) |
| BASE | Index Base (si tarif base) | 012345678 (Wh) |
| IINST | Intensité instantanée | 013 (ampères) |
| IMAX | Intensité maximale | 045 (ampères) |
| PAPP | Puissance apparente | 02980 (VA) |
| ADPS | Avertissement dépassement | 255 (si présent) |

### Exemple de trame complète

```
<STX>
<LF>ADCO 031228232024 4<CR>
<LF>OPTARIF HC.. <<CR>
<LF>ISOUSC 60 <<CR>
<LF>HCHC 003309966 *<CR>
<LF>HCHP 007772110 ,<CR>
<LF>PTEC HP..  <CR>
<LF>IINST 013 [<CR>
<LF>IMAX 045 H<CR>
<LF>PAPP 02980 4<CR>
<LF>MOTDETAT 000000 B<CR>
<ETX>
```

### Gestion des timeouts TeleInfo

```c
// Timeouts pour chaque type d'information
#define us_TELEINFO_TIME_OUT                    50   // 5 secondes
#define us_TELEINFO_OPTION_TARIFAIRE_TIME_OUT   600  // 60 secondes
#define us_TELEINFO_PERIODE_TARIFAIRE_TIME_OUT  600  // 60 secondes
#define us_TELEINFO_PUISSANCE_APP_TIME_OUT      50   // 5 secondes
#define us_TELEINFO_HCHC_APP_TIME_OUT           600  // 60 secondes
#define us_TELEINFO_HCHP_OU_BASE_APP_TIME_OUT   600  // 60 secondes

// Timer 100 ms pour incrémenter les compteurs de timeout
void vd_TeleinfoTimer(_timer_id id, pointer data_ptr, MQX_TICK_STRUCT_PTR tick_ptr)
{
    if(us_TeleinfoTimeOUT < us_TELEINFO_TIME_OUT)
        us_TeleinfoTimeOUT++;
    
    if(us_TeleinfoTimeOUTPuissanceApp < us_TELEINFO_PUISSANCE_APP_TIME_OUT)
        us_TeleinfoTimeOUTPuissanceApp++;
    
    // ... autres timeouts
}
```

Si un timeout est atteint, le système positionne un flag de défaut TeleInfo dans la table d'échange.

---

## Ethernet - Stack TCP/IP RTCS

### Description

Le client embarqué utilise la stack TCP/IP RTCS (Real-Time Communication Stack) de Freescale pour communiquer avec le serveur via Ethernet.

### Configuration Ethernet

```c
// Installation du driver socket
_io_socket_install("socket:");

// Création d'une socket TCP
uint_32 socket_handle;
socket_handle = socket(PF_INET, SOCK_STREAM, 0);

// Configuration du timeout de connexion (10 secondes)
uint_32 timeout = 10000;
setsockopt(socket_handle, SOL_TCP, OPT_CONNECT_TIMEOUT, 
           &timeout, sizeof(uint_32));

// Bind sur n'importe quelle interface locale
sockaddr_in local_addr;
local_addr.sin_family = AF_INET;
local_addr.sin_port = 0;  // Port automatique
local_addr.sin_addr.s_addr = INADDR_ANY;
bind(socket_handle, &local_addr, sizeof(local_addr));

// Connexion au serveur
sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_port = IPPORT_HTTP;  // Port 80
server_addr.sin_addr.s_addr = server_ip;
connect(socket_handle, &server_addr, sizeof(server_addr));
```

### Exemple d'ouverture de socket

```c
signed char sc_SocketOpen(
    _ip_address AdresseIP,
    unsigned long *ul_DetailErreur,
    unsigned long *ul_Socket
)
{
    uint_32 socket_handle;
    uint_32 timeout_value;
    sockaddr_in addr;
    
    // Installation du driver
    _io_socket_install("socket:");
    
    // Création de la socket
    socket_handle = socket(PF_INET, SOCK_STREAM, 0);
    if(socket_handle == RTCS_SOCKET_ERROR)
    {
        *ul_DetailErreur = RTCS_geterror(socket_handle);
        return sc_SOCKET_OPEN_ERREUR_CREATION_SOCKET;
    }
    
    // Configuration timeout (10 secondes)
    timeout_value = 10000;
    *ul_DetailErreur = setsockopt(socket_handle, SOL_TCP, 
                                  OPT_CONNECT_TIMEOUT,
                                  &timeout_value, sizeof(uint_32));
    if(*ul_DetailErreur != RTCS_OK)
    {
        return sc_SOCKET_OPEN_ERREUR_CONFIG_TIME_OUT;
    }
    
    // Bind local
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    *ul_DetailErreur = bind(socket_handle, &addr, sizeof(addr));
    if(*ul_DetailErreur != RTCS_OK)
    {
        return sc_SOCKET_OPEN_ERREUR_BIND;
    }
    
    // Connexion au serveur
    addr.sin_port = IPPORT_HTTP;
    addr.sin_addr.s_addr = AdresseIP;
    *ul_DetailErreur = connect(socket_handle, &addr, sizeof(addr));
    if(*ul_DetailErreur != RTCS_OK)
    {
        shutdown(socket_handle, FLAG_ABORT_CONNECTION);
        return sc_SOCKET_OPEN_ERREUR_OPEN_SOCKET;
    }
    
    *ul_Socket = socket_handle;
    return 0;  // Succès
}
```

### Envoi et réception HTTP

Une fois la socket ouverte, le client utilise `send()` et `recv()` pour échanger des données HTTP :

```c
// Envoi d'une requête HTTP
char request[] = "GET /api/serverinfos HTTP/1.1\r\n"
                 "Host: server\r\n"
                 "Connection: close\r\n"
                 "\r\n";
send(socket_handle, request, strlen(request), 0);

// Réception de la réponse
char buffer[1024];
int bytes_received;
bytes_received = recv(socket_handle, buffer, sizeof(buffer), 0);
```

### Contrainte critique : Single-Packet

**IMPORTANT** : Le parser HTTP du client embarqué ne supporte qu'un seul paquet TCP par réponse. Toute la réponse HTTP (headers + body) doit arriver dans un seul `recv()`.

Cette contrainte est due à :
- La mémoire limitée du microcontrôleur
- Un parser HTTP simplifié qui ne gère pas la fragmentation

Le serveur doit donc bufferiser toute la réponse avant de l'envoyer. Voir [tcp-single-packet.md](../protocol/tcp-single-packet.md) pour plus de détails.

### Fermeture de socket

```c
// Fermeture propre
shutdown(socket_handle, FLAG_CLOSE_TX);

// Ou fermeture brutale en cas d'erreur
shutdown(socket_handle, FLAG_ABORT_CONNECTION);
```

### Gestion des erreurs réseau

```c
// Récupération du code d'erreur RTCS
uint_32 error_code = RTCS_geterror(socket_handle);

// Codes d'erreur courants
// RTCSERR_SOCK_NOT_CONNECTED : Socket non connectée
// RTCSERR_SOCK_CLOSED : Socket fermée
// RTCSERR_TCP_CONN_TIMEOUT : Timeout de connexion
// RTCSERR_TCP_CONN_RESET : Connexion réinitialisée
```

---

## Synchronisation Multi-tâches

Toutes les communications utilisent des primitives de synchronisation MQX pour éviter les conflits d'accès :

### Mutex I2C

```c
// Déclaration du sémaphore
LWSEM_STRUCT lock_I2C;

// Initialisation
_lwsem_create(&lock_I2C, 1);

// Utilisation
_lwsem_wait(&lock_I2C);    // Acquérir le verrou
// ... opérations I2C ...
_lwsem_post(&lock_I2C);    // Libérer le verrou
```

### Temporisations

```c
// Délai bloquant (en millisecondes)
_time_delay(100);  // Attendre 100 ms

// Céder la main aux autres tâches
_sched_yield();
```

---

## Résumé des Paramètres

| Bus | Vitesse | Configuration | Usage |
|-----|---------|---------------|-------|
| I2C | 50 kHz | Maître, 7-bit addr | Boîtiers auxiliaires |
| SPI | 500 kHz | Mode 0, MSB first | EEPROM MAC et clé |
| UART TeleInfo | 1200 bps | 7E1 (7 bits, parité paire) | Compteur Linky |
| Ethernet | 100 Mbps | TCP/IP (RTCS) | Serveur HTTP |

---

## Références

- **Modules source** : `C/ba_i2c.c`, `C/Eepromspi.c`, `C/TeleInfo.c`, `Ethernet/GestionSocket.c`
- **Headers** : `H/ba_i2c.h`, `H/eepromspi.h`, `H/TeleInfo.h`, `Ethernet/GestionSocket.h`
- **Documentation ERDF** : "Sorties de télé-information client des appareils de comptage électroniques" (erdf_noi_cpt_02e.pdf)
- **MQX RTCS** : Freescale Real-Time Communication Stack documentation
