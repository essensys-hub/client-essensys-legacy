/* COPIE de essensys-gcc/bp/app/ba_resil.h -- ne pas modifier ici.
 * Source de reference et tests hote (NR-gcc-*) dans essensys-gcc ;
 * resynchroniser avec scripts/sync-from-essensys-gcc.sh.
 * Feature i2c-surge-protection-2026-10-007, Refs essensys-hub/essensys-board-SC944D#1 */
/*
 * ba_resil.h -- Resilience du dialogue BP <-> BA (logique pure, sans MQX).
 *
 * Feature : i2c-surge-protection-2026-10-007
 * Refs    : essensys-hub/essensys-board-SC944D#1
 *
 * Source de reference : essensys-gcc/bp/app/ba_resil.[ch], testee sur l'hote
 * (tests/resil, NR-gcc-*). client-essensys-legacy en embarque une copie
 * identique (C/ba_resil.c, H/ba_resil.h) integree a la tache BA (ba.c).
 *
 * Contenu :
 *   - 5.4 commandes conservees jusqu'a acquittement : dernier etat demande
 *     par sortie, etats maintenus appliques quel que soit leur age, volets
 *     (commandes impulsionnelles) expires apres un delai configurable ;
 *   - 5.5 resynchronisation (demarrage, retour a OK, redemarrage signale par
 *     l'octet comm_stat d'un BA 1.8) ;
 *   - 5.6 etat de sante par BA (OK / SUSPECT / ABSENT), interrogation de
 *     presence, bits 0x04/0x08/0x10 de l'index 12 (codage inchange) ;
 *   - 5.7 politique du cycle d'alimentation TE2 en dernier recours.
 *
 * Toutes les durees sont en millisecondes sur une horloge monotone fournie
 * par l'appelant (rebouclage 32 bits gere), sauf les dates des cycles TE2 en
 * secondes "epoch" (horloge murale, conservee a travers un redemarrage).
 *
 * Protocole : aucune trame nouvelle sur le bus I2C ni sur HTTP. L'index 12
 * garde son codage : bit 0 compteur, bit 1 IHM (non touches ici), bits 2..4
 * BA 0x11, 0x12, 0x13.
 */
#ifndef BA_RESIL_H
#define BA_RESIL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAR_NB_BA                   3u

/* ---- Sante et presence (5.6) ------------------------------------------- */
#define BAR_PRESENCE_POLL_OK_MS     30000u  /* BA OK : interrogation <= 60 s  */
#define BAR_RETRY_NOK_MS             5000u  /* BA non OK : essai toutes les 5 s */
#define BAR_FAILS_TO_ABSENT             3u  /* echecs consecutifs -> ABSENT   */

typedef enum {
    BAR_OK = 0,
    BAR_SUSPECT = 1,
    BAR_ABSENT = 2
} bar_health_t;

/* Bits de l'index 12 (comm_faults) -- codage 099-37 inchange. */
#define BAR_IDX12_BA_MASK           0x1Cu
uint8_t bar_idx12_bit(unsigned ba);          /* 0x04, 0x08, 0x10 */

/* ---- Commandes conservees (5.4) ----------------------------------------- */
#define BAR_VOLET_EXPIRY_MS_DEFAULT 300000u  /* 5 min */
#define BAR_NB_VOLETS                    8u

/* ---- Octet comm_stat d'un BA 1.8 (6e octet de lecture) ------------------ */
#define BAR_COMM_STAT_SIG_MASK      0xF0u
#define BAR_COMM_STAT_SIG           0xA0u    /* BA 1.7 renvoie 0x55 : jamais confondu */
#define BAR_COMM_STAT_RESTART       0x01u    /* redemarrage depuis la derniere lecture */
#define BAR_COMM_STAT_WDT           0x02u    /* redemarrage par chien de garde */
#define BAR_COMM_STAT_SSP_RESET     0x04u    /* module SSP reinitialise */
#define BAR_COMM_STAT_NACK_BUSY     0x08u    /* commande refusee (tampon occupe) */

/* Retourne 1 si l'octet est un comm_stat de BA 1.8 valide (et le recopie). */
int bar_parse_comm_stat(uint8_t byte, uint8_t *flags);

/* ---- Cycle TE2 (5.7) ----------------------------------------------------- */
#define BAR_TE2_BLOCK_MS            60000u   /* blocage persistant avant cycle */
#define BAR_TE2_CUT_MS               3000u   /* duree de coupure du +15VSP     */
#define BAR_TE2_MIN_SPACING_S        3600u   /* 1 h entre deux cycles          */
#define BAR_TE2_WINDOW_S            86400u   /* fenetre glissante de 24 h      */
#define BAR_TE2_MAX_PER_WINDOW          3u
#define BAR_TE2_EFFECT_MS           60000u   /* bus toujours bloque 60 s apres
                                                le cycle : cycle sans effet   */
#define BAR_TE2_NV_SIZE                24u

typedef enum {
    BAR_TE2_NONE = 0,
    BAR_TE2_CUT_START,      /* couper TE2 (+15VSP des BA) maintenant          */
    BAR_TE2_CUT_END         /* retablir TE2 puis resynchroniser tous les BA  */
} bar_te2_action_t;

/* Raison pour laquelle un blocage persistant ne declenche pas de cycle. */
typedef enum {
    BAR_TE2_WHY_NONE = 0,
    BAR_TE2_WHY_J58_OPEN,
    BAR_TE2_WHY_SUSPENDED,
    BAR_TE2_WHY_SPACING,
    BAR_TE2_WHY_LIMIT,
    BAR_TE2_WHY_TIME_UNKNOWN,
    BAR_TE2_WHY_SECOURU,
    BAR_TE2_WHY_WAIT_EFFECT
} bar_te2_why_t;

/* ---- Contexte ------------------------------------------------------------- */
typedef struct {
    /* Sante */
    bar_health_t health;
    uint8_t      fails;
    uint8_t      ever_ok;
    uint8_t      resync_pending;
    uint32_t     last_attempt_ms;
    uint32_t     last_ok_ms;
    uint8_t      have_attempt;

    /* Commandes en attente d'acquittement */
    uint16_t simple_on, simple_off;
    uint8_t  var_on, var_off;
    uint8_t  volet_up, volet_down;
    uint32_t volet_ts[BAR_NB_VOLETS];

    /* Trame en vol */
    uint16_t if_simple_on, if_simple_off;
    uint8_t  if_var_on, if_var_off;
    uint8_t  if_volet_up, if_volet_down;

    /* Diagnostic */
    uint32_t expired;               /* commandes de volet expirees */
    uint8_t  last_expired_mask;
    uint32_t resyncs;
    uint32_t restarts_reported;     /* drapeaux de redemarrage BA 1.8 */
} bar_ba_t;

typedef struct {
    /* Etat persistant (NV) */
    uint32_t cycles_epoch[BAR_TE2_MAX_PER_WINDOW]; /* dates des derniers cycles */
    uint32_t total_cycles;
    uint8_t  suspended;
    uint8_t  j58_open;              /* declare dans la configuration du BP */

    /* Etat volatil */
    uint8_t  cutting;
    uint8_t  awaiting_effect;
    uint32_t cut_start_ms;
    uint32_t cycle_end_ms;
    uint8_t  nv_dirty;
    bar_te2_why_t why;              /* derniere raison d'abstention */
    uint32_t blocked_reports;       /* blocages signales sans cycle */
} bar_te2_t;

typedef struct {
    bar_ba_t  ba[BAR_NB_BA];
    bar_te2_t te2;
    uint32_t  volet_expiry_ms;
    uint32_t  all_down_events;      /* defauts simultanes de tous les BA */
    uint8_t   all_down;
} bar_ctx_t;

/* Initialisation au demarrage du BP : tous les BA sont a resynchroniser. */
void bar_init(bar_ctx_t *c);

/* ---- Sante ---------------------------------------------------------------- */
/* Resultat d'un echange I2C complet avec le BA (trame + reponse).
 * Retourne 1 si le BA vient de revenir a OK (une resynchronisation est
 * alors demandee). */
int  bar_on_exchange(bar_ctx_t *c, unsigned ba, int ok, uint32_t now_ms);
bar_health_t bar_health(const bar_ctx_t *c, unsigned ba);

/* Peut-on emettre vers ce BA maintenant ? OK : toujours ; sinon un essai
 * toutes les BAR_RETRY_NOK_MS (pas de martelage d'un bus en defaut). */
int  bar_may_send(const bar_ctx_t *c, unsigned ba, uint32_t now_ms);
/* Une interrogation de presence est-elle due (aucun echange recent) ? */
int  bar_presence_due(const bar_ctx_t *c, unsigned ba, uint32_t now_ms);

/* Valeur des bits BA de l'index 12 (masque 0x1C uniquement). */
uint8_t bar_idx12_bits(const bar_ctx_t *c);
/* Applique ces bits a une valeur d'index 12 sans toucher aux autres. */
uint8_t bar_idx12_apply(const bar_ctx_t *c, uint8_t idx12);

/* ---- Resynchronisation ----------------------------------------------------- */
void bar_request_resync(bar_ctx_t *c, unsigned ba);
void bar_request_resync_all(bar_ctx_t *c);
/* Retourne 1 (et consomme la demande) si une resynchronisation doit etre
 * lancee maintenant : l'appelant marque alors configuration complete puis
 * etats des sorties a emettre. */
int  bar_take_resync(bar_ctx_t *c, unsigned ba, uint32_t now_ms);
/* comm_stat lu (6e octet). Demande une resynchronisation si le BA signale un
 * redemarrage. Retourne les drapeaux reconnus (0 si BA 1.7). */
uint8_t bar_on_comm_stat(bar_ctx_t *c, unsigned ba, uint8_t byte);

/* ---- Commandes ---------------------------------------------------------- */
/* data[0..7] : meme ordre que la trame FORCAGE_SORTIES (sans code ni CRC) :
 *   [0] simples eteint LSB, [1] simples allume LSB, [2] eteint MSB,
 *   [3] allume MSB, [4] variateurs eteint, [5] variateurs allume,
 *   [6] volets ouvre, [7] volets ferme. */
void bar_cmd_merge(bar_ctx_t *c, unsigned ba, const uint8_t data[8], uint32_t now_ms);
/* Abandonne les commandes de volet plus vieilles que le delai : retourne le
 * masque des volets expires (signales, jamais marques acquittes). */
uint8_t bar_cmd_expire(bar_ctx_t *c, unsigned ba, uint32_t now_ms);
int  bar_cmd_pending(const bar_ctx_t *c, unsigned ba);
/* Construit la trame a emettre (8 octets de donnees) et la memorise comme
 * trame en vol. Retourne 0 si rien n'est en attente. */
int  bar_cmd_build(bar_ctx_t *c, unsigned ba, uint8_t out[8]);
/* La trame en vol a ete acquittee par le BA (CRC renvoye correct). */
void bar_cmd_acked(bar_ctx_t *c, unsigned ba);

/* ---- TE2 ---------------------------------------------------------------- */
/* epoch_s = 0 si l'heure murale n'est pas connue. */
bar_te2_action_t bar_te2_step(bar_ctx_t *c, uint32_t now_ms, uint32_t epoch_s,
                              int bus_blocked, uint32_t blocked_since_ms,
                              int secouru);
void bar_te2_manual_reset(bar_ctx_t *c);
void bar_te2_set_j58_open(bar_ctx_t *c, int open);
unsigned bar_te2_cycles_in_window(const bar_ctx_t *c, uint32_t epoch_s);
uint32_t bar_te2_last_cycle_epoch(const bar_ctx_t *c);

/* Persistance (BAR_TE2_NV_SIZE octets, avec signature et somme). */
void bar_te2_save(const bar_ctx_t *c, uint8_t out[BAR_TE2_NV_SIZE]);
/* Retourne 1 si l'enregistrement est valide et a ete charge. Sinon l'etat
 * par defaut est garde (aucun cycle passe, J58 declare ouvert). */
int  bar_te2_load(bar_ctx_t *c, const uint8_t in[BAR_TE2_NV_SIZE]);

/* ---- Reinitialisation du canal I2C (5.3) -------------------------------
 * 099-37 fermait le canal (fclose) puis en rouvrait un autre (fopen) meme si
 * la fermeture echouait (fuite d'un descripteur a chaque essai, jusqu'a 50),
 * et continuait la transaction avec le pointeur passe en parametre, devenu
 * perime. Ici : un seul descripteur vivant a tout instant ; si la fermeture
 * echoue, le descripteur courant est conserve et reconfigure ; l'appelant
 * utilise TOUJOURS le descripteur retourne. */
typedef struct {
    void *(*open)(void *user);                 /* NULL si echec           */
    int   (*close)(void *user, void *handle);  /* 0 si succes             */
    int   (*configure)(void *user, void *handle); /* 0 si succes (vitesse, maitre, adresse) */
    void  *user;
} bar_i2c_ops_t;

/* Retourne le descripteur a utiliser (eventuellement NULL) ; *ok vaut 1 si
 * le canal est ouvert et configure. */
void *bar_i2c_reopen(const bar_i2c_ops_t *ops, void *current, int *ok);

#ifdef __cplusplus
}
#endif

#endif /* BA_RESIL_H */
