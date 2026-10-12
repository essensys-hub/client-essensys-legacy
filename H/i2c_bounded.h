/* COPIE de essensys-gcc/bp/i2c/i2c_bounded.h -- ne pas modifier ici.
 * Source de reference et tests hote (NR-gcc-*) dans essensys-gcc ;
 * resynchroniser avec scripts/sync-from-essensys-gcc.sh.
 * Feature i2c-surge-protection-2026-10-007, Refs essensys-hub/essensys-board-SC944D#1 */
/*
 * i2c_bounded.h -- Attentes I2C bornees dans le temps et recuperation de bus
 *                  pour le BP SC944D (MCF52259, module I2C0 sur PAS0/PAS1).
 *
 * Feature : i2c-surge-protection-2026-10-007
 * Refs    : essensys-hub/essensys-board-SC944D#1
 *
 * Ce module est volontairement independant de MQX : toute la logique
 * (bornes de temps, budget de transaction, sequence de recuperation,
 * limitation de cadence) est testee sur l'hote avec un bus et une horloge
 * simules (tests/i2c). Le materiel est atteint uniquement a travers la
 * table de fonctions i2cb_hal_t (voir i2c_bounded_mcf5225.c pour la cible).
 *
 * Bornes (spec i2c-bus-fault-containment, "Attentes I2C bornees") :
 *   - chaque attente d'un evenement du controleur : I2CB_WAIT_MAX_US
 *     (2 ms par defaut ; a relever a 2 x l'etirement d'horloge maximal
 *     mesure sur banc sur un BA 1.7 s'il depasse 1 ms, tache 5.1) ;
 *   - chaque transaction avec un BA : I2CB_TXN_MAX_US (20 ms), jamais releve.
 *
 * AUCUN binaire produit avec ce module n'est qualifie : le BP reste en 099-37
 * tant que NR-board-sc944d-3/-4 ne sont pas au vert sur banc (tache 5.12).
 */
#ifndef I2C_BOUNDED_H
#define I2C_BOUNDED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Bornes                                                                    */
/* ------------------------------------------------------------------------ */
#ifndef I2CB_WAIT_MAX_US
#define I2CB_WAIT_MAX_US            2000u   /* borne par attente (defaut)   */
#endif
#define I2CB_WAIT_MAX_US_CEILING    I2CB_TXN_MAX_US
#ifndef I2CB_TXN_MAX_US
#define I2CB_TXN_MAX_US            20000u   /* borne par transaction        */
#endif

/* Recuperation de bus */
#define I2CB_RECOVERY_CLOCKS            9u      /* impulsions d'horloge max  */
#define I2CB_RECOVERY_HALF_PERIOD_US   10u      /* ~50 kHz                   */
#define I2CB_RECOVERY_MIN_INTERVAL_MS 1000u     /* 1 tentative / s au plus   */
#define I2CB_RECOVERY_DETECT_MS        100u     /* tentative sous 100 ms     */

/* ------------------------------------------------------------------------ */
/* Couche d'abstraction materielle                                           */
/* ------------------------------------------------------------------------ */
typedef struct i2cb_hal {
    /* Horloge monotone en microsecondes (rebouclage 32 bits gere). */
    uint32_t (*now_us)(void);
    /* Attente active courte (impulsions de recuperation). */
    void     (*delay_us)(uint32_t us);

    /* Bascule PAS0 (SCL) / PAS1 (SDA) en GPIO (1) ou en fonction I2C (0). */
    void     (*pins_gpio)(int gpio);
    /* Drain ouvert emule : drive_low != 0 tire la ligne a 0, sinon relache. */
    void     (*scl_drive)(int drive_low);
    void     (*sda_drive)(int drive_low);
    /* Niveaux lus sur les broches (1 = haut). */
    int      (*scl_read)(void);
    int      (*sda_read)(void);

    /* Controleur I2C : ecriture de I2CR et lecture de I2SR. */
    void     (*i2cr_write)(uint8_t value);
    uint8_t  (*i2sr_read)(void);
} i2cb_hal_t;

/* Bits du registre I2CR / I2SR du MCF52xx utilises ici. */
#define I2CB_I2CR_IEN   0x80u
#define I2CB_I2SR_IBB   0x20u

/* Installe la HAL (une seule instance : un seul bus BA sur le BP). */
void i2cb_init(const i2cb_hal_t *hal);
const i2cb_hal_t *i2cb_hal(void);

/* ------------------------------------------------------------------------ */
/* Attentes bornees                                                          */
/* ------------------------------------------------------------------------ */
typedef struct {
    uint32_t start_us;
    uint32_t limit_us;
} i2cb_deadline_t;

/* Borne par attente effectivement appliquee (reglable, plafonnee a la
 * borne de transaction). Retourne la valeur retenue. */
uint32_t i2cb_set_wait_max_us(uint32_t us);
uint32_t i2cb_wait_max_us(void);

/* Debut / fin d'une transaction avec un BA (budget I2CB_TXN_MAX_US).
 * Hors transaction, seule la borne par attente s'applique. */
void i2cb_txn_begin(void);
void i2cb_txn_end(void);
int  i2cb_txn_expired(void);
uint32_t i2cb_txn_elapsed_us(void);

/* Arme une attente : limite = min(borne par attente, budget restant). */
void i2cb_wait_begin(i2cb_deadline_t *dl);
/* Non nul si l'attente a depasse sa limite. */
int  i2cb_wait_expired(const i2cb_deadline_t *dl);

/* Identifiants des sites d'attente du pilote (diagnostic). */
enum {
    I2CB_SITE_FLUSH = 1,     /* IO_IOCTL_FLUSH_OUTPUT                        */
    I2CB_SITE_STOP_IDLE,     /* IO_IOCTL_I2C_STOP : attente bus libre        */
    I2CB_SITE_RX_ADDR,       /* emission adresse en lecture                  */
    I2CB_SITE_TX_ADDR,       /* emission adresse en ecriture                 */
    I2CB_SITE_RXTX_BYTE,     /* octet de donnees                             */
    I2CB_SITE_COUNT
};

/* Signale le depassement d'une attente (compteurs + demande de recuperation). */
void i2cb_note_timeout(int site);

/* ------------------------------------------------------------------------ */
/* Recuperation de bus (tache 5.2)                                           */
/* ------------------------------------------------------------------------ */
typedef enum {
    I2CB_REC_NOT_NEEDED = 0, /* lignes hautes : rien a faire                  */
    I2CB_REC_RATE_LIMITED,   /* derniere tentative il y a moins de 1 s        */
    I2CB_REC_RELEASED,       /* bus libere par la sequence                    */
    I2CB_REC_STILL_STUCK     /* une ligne reste basse apres la sequence       */
} i2cb_rec_result_t;

/* Lit les niveaux en GPIO (sans rien piloter). Bit0 = SCL haut, bit1 = SDA haut. */
uint8_t i2cb_sample_lines(void);

/* Tentative de recuperation, limitee a une par I2CB_RECOVERY_MIN_INTERVAL_MS :
 *   PAS0/PAS1 en GPIO, lecture des niveaux, jusqu'a 9 impulsions sur SCL tant
 *   que SDA est basse, condition STOP, retour en fonction I2C, I2CR = 0 puis
 *   I2CR = IEN. now_ms est l'horloge monotone de l'appelant en ms. */
i2cb_rec_result_t i2cb_recover(uint32_t now_ms);

/* A appeler periodiquement (<= 100 ms) bus au repos : declenche une
 * recuperation si une ligne est basse ou si un depassement a ete signale. */
i2cb_rec_result_t i2cb_poll(uint32_t now_ms);

/* Non nul tant que le bus est considere bloque (derniere observation). */
int  i2cb_bus_blocked(void);
/* Instant (ms, horloge de l'appelant) du debut du blocage courant. */
uint32_t i2cb_blocked_since_ms(void);

/* ------------------------------------------------------------------------ */
/* Statistiques (console Espion)                                             */
/* ------------------------------------------------------------------------ */
typedef struct {
    uint32_t timeouts[I2CB_SITE_COUNT];  /* depassements par site            */
    uint32_t txn_timeouts;               /* transactions > I2CB_TXN_MAX_US    */
    uint32_t recoveries;                 /* sequences de recuperation lancees */
    uint32_t recoveries_ok;              /* ... ayant libere le bus           */
    uint32_t recovery_clocks;            /* impulsions emises (cumul)         */
    uint32_t max_wait_us;                /* plus longue attente observee      */
} i2cb_stats_t;

const i2cb_stats_t *i2cb_stats(void);
void i2cb_stats_reset(void);

/* HAL cible MCF52259 (i2c_bounded_mcf5225.c), a installer au demarrage. */
void i2cb_init_mcf5225(void);

/* Remise a zero complete (tests hote). */
void i2cb_reset_state(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BOUNDED_H */
