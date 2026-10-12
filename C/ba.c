#include <mqx.h>
#include <bsp.h>
#include <timer.h>
#include <mutex.h>
#include <message.h>
#include "application.h"	//xxx

#include <i2c.h>

#include "TableEchange.h"
#include "TableEchangeAcces.h"
#include "EspionRS.h"
#include "ba_i2c.h"
#include "global.h"
#include "crc.h"
#include "ba_resil.h"
#include "i2c_bounded.h"

#define extern
#include "ba.h"
#undef extern


// Variables locales utilisées par le module

// Contient les informations sur les BA de la table d'échange
unsigned char uc_Tb_Borniers_Data[Scenario - Variateurs_PDV_Conf];		// la taille doit correspondre à la zone mémoire de la table d'échange !

// Gestion des émissions / répétitions
typedef struct {
	unsigned short	b_Forcage		:1;		// 1= message à émettre
	unsigned short	b_Conf			:1;		// 1= message à émettre
	unsigned short	b_Extinction	:1;		// 1= message à émettre
	unsigned short	b_TpsAction		:1;		// 1= message à émettre
	unsigned short	b_Action		:1;		// 1= message à émettre
	unsigned short	b_nu3			:1;		// non utilisé
	unsigned short	b_nu2			:1;		// non utilisé
	unsigned short	b_nu1			:1;		// non utilisé
	unsigned short	o_Repete		:8;		// compteur de répétitions ou d'erreurs
}tst_Com_BA;
tst_Com_BA Tb_Com_BA[uc_NB_BOITIER_AUXILIAIRE];

// Contient les actions à envoyer - ORDRE champs -> ORDRE I2C - Ne pas modifier !!!
typedef union {
	struct {
		unsigned char	nu1				:1;		// non utilisé
		unsigned char	nu2				:1;		// non utilisé
		unsigned char	nu3				:1;		// non utilisé
		unsigned char	nu4				:1;		// non utilisé
		unsigned char	nu5				:1;		// non utilisé
		unsigned char	ForceAllumage	:1;		// 1= allumage forcé
		unsigned char	BloqueVolets	:1;		// 1= blocage des volets
		unsigned char	Secouru			:1;		// 1= mode secouru
	}b;
	unsigned char	Octet;
}Actions_BA;
Actions_BA Tb_Actions_Cde_BA[uc_NB_BOITIER_AUXILIAIRE];			// actions en cours pour chaque bornier
Actions_BA Tb_Actions_Ack_BA[uc_NB_BOITIER_AUXILIAIRE];			// actions acquittées pour chaque bornier


//--------------------------------------------------------------------------
// Feature i2c-surge-protection-2026-10-007 (Refs essensys-hub/essensys-board-SC944D#1)
//
// Tache BA reecrite autour de ba_resil (logique testee sur hote, NR-gcc-4 a 11) :
//  - 5.4 : plus aucune commande "marquee acquittee" apres des echecs
//    (099-37 : ba.c:202-209, 313-316) ; les forcages sont fusionnes dans un
//    magasin par BA (dernier etat demande par sortie), les configurations et
//    l'octet d'actions restent a emettre jusqu'a leur acquittement ; les
//    commandes de volet expirent apres 5 min (signalees, jamais acquittees) ;
//  - 5.5 : resynchronisation complete (configuration puis etats) au demarrage,
//    au retour d'un BA a l'etat OK et sur le drapeau de redemarrage du
//    6e octet (comm_stat) d'un BA 1.8 ;
//  - 5.6 : etat OK / SUSPECT / ABSENT, interrogation de presence (trame
//    ACTIONS inchangee) au moins toutes les 30 s, bits de l'index 12 mis a
//    jour sans attendre une commande (codage inchange) ;
//  - 5.2 / 5.7 : recuperation de bus (i2cb_poll) et cycle TE2 en dernier
//    recours (bar_te2_step).
// Aucune trame nouvelle sur le bus I2C ; protocole HTTP inchange.
//--------------------------------------------------------------------------

bar_ctx_t st_BA_Resil;		// etat de resilience (lu par la console Espion)

#define ul_PERIODE_POLL_BUS_ms		50		// surveillance du bus au repos (< 100 ms)
#define ul_EPOCH_MIN_VALIDE			1577836800UL	// 2020-01-01 : en dessous, heure non reglee

// Horloge monotone en ms (rebouclage gere par ba_resil)
static unsigned long ul_MaintenantMs(void)
{
	TIME_STRUCT l_st_Temps;
	_time_get_elapsed(&l_st_Temps);
	return (unsigned long)(l_st_Temps.SECONDS * 1000UL + l_st_Temps.MILLISECONDS);
}

// Heure murale (s depuis 1970) ou 0 si non reglee
static unsigned long ul_EpochSecondes(void)
{
	TIME_STRUCT l_st_Temps;
	_time_get(&l_st_Temps);
	return (l_st_Temps.SECONDS >= ul_EPOCH_MIN_VALIDE) ? (unsigned long)l_st_Temps.SECONDS : 0UL;
}

// Ajoute le CRC en fin de trame ; retourne la longueur totale
static unsigned short us_AjouterCRC(unsigned char *p_uc_Trame, unsigned short us_Longueur)
{
	unsigned short l_us_CRC;
	l_us_CRC = us_CalculerCRCSurTrame(p_uc_Trame, us_Longueur);
	p_uc_Trame[us_Longueur] = (l_us_CRC & 0xFF);
	p_uc_Trame[us_Longueur+1] = ((l_us_CRC>>8) & 0xFF);
	return (unsigned short)(us_Longueur + 2);
}

// Emet une trame complete (CRC compris) et met a jour l'etat de sante du BA
static signed long sl_EnvoyerTrameBA(unsigned short us_BA, unsigned char *p_uc_Trame, unsigned short us_Longueur)
{
	signed long l_sl_result;
	unsigned char l_uc_ReponseRecue;
	unsigned char l_uc_CommStat;
	unsigned char l_uc_Flags;

	ul_EspionTacheBACompteurEmissionI2C[us_BA]++;
	l_sl_result = sl_fct_write_polled(fd_BA_I2C, (uchar)us_BA, p_uc_Trame, us_Longueur, &l_uc_ReponseRecue, &l_uc_CommStat);
	if(bar_on_exchange(&st_BA_Resil, us_BA, l_sl_result == I2C_OK, ul_MaintenantMs()) != 0)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"BA %i de retour -> resynchronisation\n", us_BA);
	}
	if(l_sl_result == I2C_OK)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA emission OK\n");
		Tb_Com_BA[us_BA].o_Repete = 0;
		l_uc_Flags = bar_on_comm_stat(&st_BA_Resil, us_BA, l_uc_CommStat);
		if((l_uc_Flags & (BAR_COMM_STAT_RESTART | BAR_COMM_STAT_WDT)) != 0)
		{
			vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"BA %i signale un redemarrage (comm_stat 0x%X) -> resynchronisation\n", us_BA, l_uc_CommStat);
		}
	}
	else
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA emission PB %d\n", l_sl_result);
		ul_EspionTacheBACompteurErreurI2C[us_BA]++;
		if(Tb_Com_BA[us_BA].o_Repete < 0xFF)	Tb_Com_BA[us_BA].o_Repete++;	// espion uniquement
	}
	return l_sl_result;
}

// Prepare une trame de configuration (8 ou 16 octets de Tb_Echange) ; retourne 1 si un changement est detecte
static unsigned char uc_PreparerTrameConf(unsigned char *p_uc_Trame, unsigned short us_Debut, unsigned short us_Nb)
{
	unsigned short l_us_k;
	unsigned short l_us_j = 1;
	unsigned char l_uc_Change = 0;

	for(l_us_k = us_Debut; l_us_k < us_Debut + us_Nb; l_us_k++)
	{
		if(Tb_Echange[l_us_k] != uc_Tb_Borniers_Data[l_us_k - Variateurs_PDV_Conf])
		{
			l_uc_Change = 1;
			uc_Tb_Borniers_Data[l_us_k - Variateurs_PDV_Conf] = Tb_Echange[l_us_k];
		}
		p_uc_Trame[l_us_j++] = uc_Tb_Borniers_Data[l_us_k - Variateurs_PDV_Conf];
	}
	return l_uc_Change;
}

void Boitiers_task(uint_32 dummy)
{
	signed long l_sl_param;
	_queue_id l_BA_qid;
	unsigned short l_us_i, l_us_j;
	unsigned char l_uc_buffer[uc_I2C_BUFFER_SIZE];
	unsigned char l_uc_Donnees[8];
	static COMMANDE_MESSAGE_PTR	s_msg_ptr = NULL;
	unsigned char l_uc_Compteur;
	unsigned short l_us_Longueur;
	unsigned long l_ul_Maintenant;
	unsigned long l_ul_DernierPollBus = 0;
	unsigned char l_uc_Expires;
	unsigned char l_uc_EmisCeTour[uc_NB_BOITIER_AUXILIAIRE];
	bar_te2_action_t l_e_ActionTE2;
	bar_te2_why_t l_e_RaisonTE2Precedente = BAR_TE2_WHY_NONE;
	

	uc_EspionTacheBAEtat = 1;
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"INIT\n");
	
	// I2C transaction lock 
	_lwsem_create(&lock_I2C, 1);	// xxx tester retour

	// Pilote I2C a attentes bornees : HAL (horloge, PAS0/PAS1) du MCF52259
	i2cb_init_mcf5225();

	// Etat de resilience ; etat persistant du cycle TE2 relu en FLASH
	bar_init(&st_BA_Resil);
	if(uc_TE2_NVValide != 0)
	{
		if(bar_te2_load(&st_BA_Resil, uc_TE2_NV) == 0)
		{
			vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"TE2 : enregistrement FLASH invalide -> J58 declare ouvert, aucun cycle\n");
		}
	}

	// Open the I2C driver          
	fd_BA_I2C = fopen(I2C_DEVICE_POLLED, NULL);
	if(fd_BA_I2C == NULL) 
	{
		DETECTION_ERREUR_TACHE_BA_OUVERTURE_I2C;
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Erreur driver I2C!\n");
		_task_block ();
	}

	uc_EspionTacheBAEtat = 2;
	l_sl_param = 50000;
	if(ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_BAUD, &l_sl_param) != I2C_OK)
	{
		DETECTION_ERREUR_TACHE_BA_CONFIG_VITESSE;
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Erreur config vitesse!\n");
		_task_block ();
	}

	// BA: Set master mode
	uc_EspionTacheBAEtat = 3;
	if(ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_MASTER_MODE, NULL) != I2C_OK)
	{
		DETECTION_ERREUR_TACHE_BA_CONFIG_MAITRE;
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Erreur config master!\n");
		_task_block ();
	}

	uc_EspionTacheBAEtat = 4;
	l_sl_param = I2C_BUS_ADDRESS_BP;	// adresse du boitier principal
	if(ioctl(fd_BA_I2C, IO_IOCTL_I2C_SET_STATION_ADDRESS, &l_sl_param) != I2C_OK)
	{
		DETECTION_ERREUR_TACHE_BA_CONFIG_ADRESSE;
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Erreur config adresse!\n");
		_task_block ();
	}

	// Open a message queue
	uc_EspionTacheBAEtat = 5;
	l_BA_qid = _msgq_open(BA_QUEUE_ID, 0);
	if(l_BA_qid == MSGQ_NULL_QUEUE_ID)
	{
		DETECTION_ERREUR_TACHE_BA_CREATION_MESSAGE_QUEUE;
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Erreur open queue!\n");
		_task_block ();
	}	

	// Init des donnees a partir de la table d'echange
	uc_EspionTacheBAEtat = 6;
	for(l_us_i=0, l_us_j=Variateurs_PDV_Conf; l_us_j < Scenario ; l_us_i++, l_us_j++)
	{
		uc_Tb_Borniers_Data[l_us_i] = Tb_Echange[l_us_j];
	}
	for(l_us_i=0; l_us_i < uc_NB_BOITIER_AUXILIAIRE ; l_us_i++)
	{
		Tb_Com_BA[l_us_i].b_Forcage = 0;
		Tb_Com_BA[l_us_i].b_Conf = 0;
		Tb_Com_BA[l_us_i].b_Extinction = 0;
		Tb_Com_BA[l_us_i].b_TpsAction = 0;
		Tb_Com_BA[l_us_i].b_Action = 0;
		Tb_Com_BA[l_us_i].o_Repete = 0;
		
		Tb_Actions_Cde_BA[l_us_i].Octet = 0;
		Tb_Actions_Ack_BA[l_us_i].Octet = 0;
	}

	uc_EspionTacheBAEtat = 7;
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"START\n");
	
	for (;;)
	{
		ul_EspionTacheBACompteurActivite++;
		l_ul_Maintenant = ul_MaintenantMs();
		for(l_us_i=0; l_us_i<uc_NB_BOITIER_AUXILIAIRE; l_us_i++)	l_uc_EmisCeTour[l_us_i] = 0;

		// 1. Forcages recus de la tache principale -> dernier etat demande par sortie
		uc_EspionTacheBAEtat = 9;
		while((s_msg_ptr = _msgq_poll(l_BA_qid)) != NULL)
		{
			ul_EspionTachePrincipaleNbMessagesBARecus++;
			l_us_i = s_msg_ptr->DATA[0];
			if(l_us_i < uc_NB_BOITIER_AUXILIAIRE)
			{
				for(l_uc_Compteur = 0; l_uc_Compteur < 8; l_uc_Compteur++)	l_uc_Donnees[l_uc_Compteur] = s_msg_ptr->DATA[1 + l_uc_Compteur];
				bar_cmd_merge(&st_BA_Resil, l_us_i, l_uc_Donnees, l_ul_Maintenant);
				Tb_Com_BA[l_us_i].b_Forcage = 1;
			}
			_msg_free(s_msg_ptr);
			s_msg_ptr = NULL;
		}

		// 2. Expirations (volets) et resynchronisations
		uc_EspionTacheBAEtat = 10;
		for(l_us_i=0; l_us_i<uc_NB_BOITIER_AUXILIAIRE; l_us_i++)
		{
			l_uc_Expires = bar_cmd_expire(&st_BA_Resil, l_us_i, l_ul_Maintenant);
			if(l_uc_Expires != 0)
			{
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"BA %i : commande(s) de volet EXPIREE(S) (masque 0x%X), non appliquee(s)\n", l_us_i, l_uc_Expires);
			}
			if(bar_take_resync(&st_BA_Resil, l_us_i, l_ul_Maintenant) != 0)
			{
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"BA %i : resynchronisation (configuration puis etats)\n", l_us_i);
				Tb_Com_BA[l_us_i].b_Conf = 1;
				Tb_Com_BA[l_us_i].b_Extinction = 1;
				Tb_Com_BA[l_us_i].b_TpsAction = 1;
				Tb_Com_BA[l_us_i].b_Action = 1;
			}
		}

		// 3. Configuration (modes variateurs, temps d'extinction, temps des volets)
		//    avant les etats ; jamais abandonnee avant acquittement
		uc_EspionTacheBAEtat = 11;
		for(l_us_i=0; l_us_i<uc_NB_BOITIER_AUXILIAIRE; l_us_i++)
		{
			if(uc_PreparerTrameConf(l_uc_buffer, Variateurs_PDV_Conf + (l_us_i <<3), 8) != 0)	Tb_Com_BA[l_us_i].b_Conf = 1;
			if(Tb_Com_BA[l_us_i].b_Conf == 1 && bar_may_send(&st_BA_Resil, l_us_i, ul_MaintenantMs()) != 0)
			{
				l_uc_buffer[0] = uc_TRAME_BA_CONF_SORTIES;
				l_us_Longueur = us_AjouterCRC(l_uc_buffer, 9);
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : envoi configuration des sorties au boitier %i (%i)\n", l_us_i, Tb_Com_BA[l_us_i].o_Repete);
				l_uc_EmisCeTour[l_us_i] = 1;
				if(sl_EnvoyerTrameBA(l_us_i, l_uc_buffer, l_us_Longueur) == I2C_OK)	Tb_Com_BA[l_us_i].b_Conf = 0;
			}

			if(uc_PreparerTrameConf(l_uc_buffer, Lampes_PDV_Temps + (l_us_i <<4), 16) != 0)	Tb_Com_BA[l_us_i].b_Extinction = 1;
			if(Tb_Com_BA[l_us_i].b_Extinction == 1 && bar_may_send(&st_BA_Resil, l_us_i, ul_MaintenantMs()) != 0)
			{
				l_uc_buffer[0] = uc_TRAME_BA_TPS_EXTINCTION;
				l_us_Longueur = us_AjouterCRC(l_uc_buffer, 17);
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : envoi reglage des temps d extinction au boitier %i (%i)\n", l_us_i, Tb_Com_BA[l_us_i].o_Repete);
				l_uc_EmisCeTour[l_us_i] = 1;
				if(sl_EnvoyerTrameBA(l_us_i, l_uc_buffer, l_us_Longueur) == I2C_OK)	Tb_Com_BA[l_us_i].b_Extinction = 0;
			}

			if(uc_PreparerTrameConf(l_uc_buffer, Volets_PDV_Temps + (l_us_i <<3), 8) != 0)	Tb_Com_BA[l_us_i].b_TpsAction = 1;
			if(Tb_Com_BA[l_us_i].b_TpsAction == 1 && bar_may_send(&st_BA_Resil, l_us_i, ul_MaintenantMs()) != 0)
			{
				l_uc_buffer[0] = uc_TRAME_BA_TPS_ACTION;
				l_us_Longueur = us_AjouterCRC(l_uc_buffer, 9);
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : envoi reglage des temps d action au boitier %i (%i)\n", l_us_i, Tb_Com_BA[l_us_i].o_Repete);
				l_uc_EmisCeTour[l_us_i] = 1;
				if(sl_EnvoyerTrameBA(l_us_i, l_uc_buffer, l_us_Longueur) == I2C_OK)	Tb_Com_BA[l_us_i].b_TpsAction = 0;
			}
		}

		// 4. Octet d'actions (secouru, blocage volets, allumage force)
		uc_EspionTacheBAEtat = 8;
		for (l_us_i=0; l_us_i<uc_NB_BOITIER_AUXILIAIRE; l_us_i++)
		{
			Tb_Actions_Cde_BA[l_us_i].b.Secouru = 0;
			if(st_EchangeStatus.uc_Secouru != 0 || uc_FlagResetBP != 0)		// mode secouru => informer les boitiers pour qu'ils fassent leur sauvegarde
			{																// ou Reset BP programmé => sauvegarde état BA
				Tb_Actions_Cde_BA[l_us_i].b.Secouru = 1;
			}
			Tb_Actions_Cde_BA[l_us_i].b.BloqueVolets = st_EchangeStatus.uc_BloquerVolets;		// blocage des volets
			Tb_Actions_Cde_BA[l_us_i].b.ForceAllumage = st_EchangeStatus.uc_ForcerAllumage;		// allumage forcé
			if(us_CompteurPilotageSireneInterieure_sec >= us_DUREE_PILOTAGE_SIRENE_INTERIEURE_sec)	// Suivre le pilotage de la sirène intérieure
			{
				Tb_Actions_Cde_BA[l_us_i].b.ForceAllumage = 0;
			}
			if(Tb_Actions_Cde_BA[l_us_i].Octet != Tb_Actions_Ack_BA[l_us_i].Octet)	Tb_Com_BA[l_us_i].b_Action = 1;

			if(Tb_Com_BA[l_us_i].b_Action == 1 && bar_may_send(&st_BA_Resil, l_us_i, ul_MaintenantMs()) != 0)
			{
				l_uc_buffer[0] = uc_TRAME_BA_ACTIONS;
				l_uc_buffer[1] = Tb_Actions_Cde_BA[l_us_i].Octet;
				l_us_Longueur = us_AjouterCRC(l_uc_buffer, 2);
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : envoi action au boitier %i (%i) -> %d\n", l_us_i, Tb_Com_BA[l_us_i].o_Repete, l_uc_buffer[1]);
				l_uc_EmisCeTour[l_us_i] = 1;
				if(sl_EnvoyerTrameBA(l_us_i, l_uc_buffer, l_us_Longueur) == I2C_OK)
				{
					Tb_Actions_Ack_BA[l_us_i].Octet = l_uc_buffer[1];
					Tb_Com_BA[l_us_i].b_Action = 0;
				}
			}
		}

		// 5. Forcage des sorties : dernier etat demande, conserve jusqu'a acquittement
		uc_EspionTacheBAEtat = 12;
		for(l_us_i=0; l_us_i<uc_NB_BOITIER_AUXILIAIRE; l_us_i++)
		{
			if(bar_cmd_pending(&st_BA_Resil, l_us_i) != 0 && bar_may_send(&st_BA_Resil, l_us_i, ul_MaintenantMs()) != 0)
			{
				l_uc_buffer[0] = uc_TRAME_BA_FORCAGE_SORTIES;
				(void)bar_cmd_build(&st_BA_Resil, l_us_i, &l_uc_buffer[1]);
				l_us_Longueur = us_AjouterCRC(l_uc_buffer, 9);
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : Forçage sorties -> BA %i (%i) -> %d", l_us_i, Tb_Com_BA[l_us_i].o_Repete, l_uc_buffer[0]);
				for(l_uc_Compteur = 1 ; l_uc_Compteur < 11; l_uc_Compteur++)
				{
					vd_EspionRS_PrintfSansHorodatage(uc_ESPION_TACHE_BA_ACTIVITE,".%d", l_uc_buffer[l_uc_Compteur]);
				}
				vd_EspionRS_PrintfSansHorodatage(uc_ESPION_TACHE_BA_ACTIVITE,"\n");
				l_uc_EmisCeTour[l_us_i] = 1;
				if(sl_EnvoyerTrameBA(l_us_i, l_uc_buffer, l_us_Longueur) == I2C_OK)
				{
					bar_cmd_acked(&st_BA_Resil, l_us_i);
					if(bar_cmd_pending(&st_BA_Resil, l_us_i) == 0)	Tb_Com_BA[l_us_i].b_Forcage = 0;
				}
			}
		}

		// 6. Interrogation de presence (trame ACTIONS courante, sans effet sur le BA)
		uc_EspionTacheBAEtat = 13;
		for(l_us_i=0; l_us_i<uc_NB_BOITIER_AUXILIAIRE; l_us_i++)
		{
			l_ul_Maintenant = ul_MaintenantMs();
			if(l_uc_EmisCeTour[l_us_i] == 0 && bar_presence_due(&st_BA_Resil, l_us_i, l_ul_Maintenant) != 0
			   && bar_may_send(&st_BA_Resil, l_us_i, l_ul_Maintenant) != 0)
			{
				l_uc_buffer[0] = uc_TRAME_BA_ACTIONS;
				l_uc_buffer[1] = Tb_Actions_Ack_BA[l_us_i].Octet;
				l_us_Longueur = us_AjouterCRC(l_uc_buffer, 2);
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : interrogation de presence BA %i\n", l_us_i);
				(void)sl_EnvoyerTrameBA(l_us_i, l_uc_buffer, l_us_Longueur);
			}
		}

		// 7. Surveillance du bus au repos et recuperation (<= 1 tentative / s)
		uc_EspionTacheBAEtat = 14;
		l_ul_Maintenant = ul_MaintenantMs();
		if((unsigned long)(l_ul_Maintenant - l_ul_DernierPollBus) >= ul_PERIODE_POLL_BUS_ms)
		{
			l_ul_DernierPollBus = l_ul_Maintenant;
			_lwsem_wait(&lock_I2C);
			switch(i2cb_poll(l_ul_Maintenant))
			{
				case I2CB_REC_RELEASED:
					vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Bus I2C : recuperation reussie\n");
				break;
				case I2CB_REC_STILL_STUCK:
					vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"Bus I2C : ligne toujours basse apres recuperation\n");
				break;
				default:
				break;
			}
			_lwsem_post(&lock_I2C);

			// 8. Cycle TE2 en dernier recours
			l_e_ActionTE2 = bar_te2_step(&st_BA_Resil, l_ul_Maintenant, ul_EpochSecondes(),
			                             i2cb_bus_blocked(), i2cb_blocked_since_ms(),
			                             st_EchangeStatus.uc_Secouru != 0);
			if(l_e_ActionTE2 == BAR_TE2_CUT_START)
			{
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"TE2 : bus bloque depuis plus de 60 s -> coupure +15VSP des BA (cycle %d, date %lu)\n",
				                   st_BA_Resil.te2.total_cycles, bar_te2_last_cycle_epoch(&st_BA_Resil));
				lwgpio_set_value(&IO_DOUT_15VSP_AlimBA, LWGPIO_VALUE_LOW);
			}
			else if(l_e_ActionTE2 == BAR_TE2_CUT_END)
			{
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"TE2 : retablissement +15VSP -> resynchronisation de tous les BA\n");
				lwgpio_set_value(&IO_DOUT_15VSP_AlimBA, LWGPIO_VALUE_HIGH);
			}
			if(st_BA_Resil.te2.why != l_e_RaisonTE2Precedente)
			{
				l_e_RaisonTE2Precedente = st_BA_Resil.te2.why;
				if(l_e_RaisonTE2Precedente != BAR_TE2_WHY_NONE)
				{
					vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"TE2 : bus bloque, aucun cycle (raison %d, voir console : resilience BA)\n", l_e_RaisonTE2Precedente);
				}
			}
			if(st_BA_Resil.te2.nv_dirty != 0)
			{
				bar_te2_save(&st_BA_Resil, uc_TE2_NV);
				st_BA_Resil.te2.nv_dirty = 0;
				uc_DemandeSauvegardeTE2 = 1;		// ecriture FLASH par la tache principale
			}
		}

		// 9. Index 12 (comm_faults) : codage inchange, bit positionne quand le BA est ABSENT
		uc_EspionTacheBAEtat = 15;
		st_EchangeStatus.uc_Defboitier1 = (bar_health(&st_BA_Resil, 0) == BAR_ABSENT) ? 1 : 0;
		st_EchangeStatus.uc_Defboitier2 = (bar_health(&st_BA_Resil, 1) == BAR_ABSENT) ? 1 : 0;
		st_EchangeStatus.uc_Defboitier3 = (bar_health(&st_BA_Resil, 2) == BAR_ABSENT) ? 1 : 0;

		_sched_yield();		// passe la main à la tache suivante
		uc_EspionTacheBAEtat = 16;
	}	   // fin boucle tache de fond permanente
} 

// L'applicatif va forcer l'état des sorties selon les cas
// 		-> exécution d'un scénario
// Les ordres de pilotage sont stockés dans Tb_Echangeboitier
// Dans la tache principale, si Tb_Echangeboitier contient des ordres à envoyer (!= 0), ces ordres sont envoyés à la tache BA via un message
// Pourquoi un message ?
// 		Après émission, les ordres sont remis à 0
//		Pendant émission d'autres ordres peuvent venir...
//		Le fait d'envoyer un message permet de remettre à 0 tout de suite le tableau Tb_Echangeboitier
//		Tout en envoyant son état par message, ce qui permet de ne pas perdre un éventuel ordre qui arriverait tout de suite après
// xxx mutex sur us_Tb_Echangeboitier ? a priori non car change par scenarios appelle depuis le main donc pas d'acces mutithreads...
void vd_UpdateOrdresPilotageSortiesBA(void)
{
	unsigned char l_uc_CompteurBoitier;
	unsigned char l_uc_IndiceData;
	static unsigned char s_uc_FlagErreurDetectee = 0;	// Pour éviter x messages en cas d'erreur...
	COMMANDE_MESSAGE_PTR l_msg_ptr;
	
	// synchronisation des écritures vers les BA
	for(l_uc_CompteurBoitier=0; l_uc_CompteurBoitier<uc_NB_BOITIER_AUXILIAIRE; l_uc_CompteurBoitier++)			// boucle sur tous les boitiers
	//l_uc_CompteurBoitier = uc_BOITIER_PIECES_EAU;	//zzz
	{
		if(us_Tb_Echangeboitier[us_SSimples_BA1_CdeEteint+l_uc_CompteurBoitier] != 0 ||
		   us_Tb_Echangeboitier[us_SSimples_BA1_CdeAllume+l_uc_CompteurBoitier] != 0 ||
		   us_Tb_Echangeboitier[us_SVariateurs_BA1_CdeEteint+l_uc_CompteurBoitier] != 0 ||
		   us_Tb_Echangeboitier[us_SVariateurs_BA1_CdeAllume+l_uc_CompteurBoitier] != 0 ||
		   us_Tb_Echangeboitier[us_SVolets_BA1_Cdeouvre+l_uc_CompteurBoitier] != 0 ||
		   us_Tb_Echangeboitier[us_SVolets_BA1_CdeFerme+l_uc_CompteurBoitier] != 0)
		{
			// au moins 1 forçage à transmettre
			if(_msg_available(message_pool) != 0) 
			{
				l_msg_ptr = (COMMANDE_MESSAGE_PTR)_msg_alloc(message_pool);
				if(l_msg_ptr == NULL)
				{
					ul_EspionTachePrincipaleNbMessagesBAErreurs1++;
					DETECTION_ERREUR_TACHE_PRINCIPALE_MESSAGE_ALLOC;
					if(s_uc_FlagErreurDetectee == 0)
					{
						s_uc_FlagErreurDetectee = 1;
						vd_EspionRS_Printf(uc_ESPION_TACHE_PRINCIPALE_ERREUR,"Error creation message!\n");
					}
				}
				else
				{
					l_msg_ptr->HEADER.TARGET_QID = _msgq_get_id(0, BA_QUEUE_ID);
					l_msg_ptr->HEADER.SIZE = sizeof(COMMANDE_MESSAGE) + 1;
					// renseigne zone data
					l_msg_ptr->DATA[0] = (uchar)l_uc_CompteurBoitier;		// numero de BA
					l_uc_IndiceData = 1;		// pointeur dans la trame
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SSimples_BA1_CdeEteint+l_uc_CompteurBoitier] & 0xFF);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SSimples_BA1_CdeAllume+l_uc_CompteurBoitier] & 0xFF);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SSimples_BA1_CdeEteint+l_uc_CompteurBoitier] >> 8);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SSimples_BA1_CdeAllume+l_uc_CompteurBoitier] >> 8);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SVariateurs_BA1_CdeEteint+l_uc_CompteurBoitier] & 0xFF);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SVariateurs_BA1_CdeAllume+l_uc_CompteurBoitier] & 0xFF);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SVolets_BA1_Cdeouvre+l_uc_CompteurBoitier] & 0xFF);
					l_msg_ptr->DATA[l_uc_IndiceData++] = (uchar) (us_Tb_Echangeboitier[us_SVolets_BA1_CdeFerme+l_uc_CompteurBoitier] & 0xFF);
					//xxx pas tout envoyé ???
			
					if(_msgq_send(l_msg_ptr) == FALSE)
					{
						ul_EspionTachePrincipaleNbMessagesBAErreurs2++;
						DETECTION_ERREUR_TACHE_PRINCIPALE_MESAGE_SEND;
						if(s_uc_FlagErreurDetectee == 0)
						{
							s_uc_FlagErreurDetectee = 1;
							vd_EspionRS_Printf(uc_ESPION_TACHE_PRINCIPALE_ERREUR,"Error envoi message!\n");
						}
						_msg_free(l_msg_ptr);
					}
					else
					{
						ul_EspionTachePrincipaleNbMessagesBAEnvoyes++;
						s_uc_FlagErreurDetectee = 0;
						// forçages transmis : raz demandes
						us_Tb_Echangeboitier[us_SSimples_BA1_CdeEteint+l_uc_CompteurBoitier] = 0;
						us_Tb_Echangeboitier[us_SSimples_BA1_CdeAllume+l_uc_CompteurBoitier] = 0;
						us_Tb_Echangeboitier[us_SVariateurs_BA1_CdeEteint+l_uc_CompteurBoitier] = 0;
						us_Tb_Echangeboitier[us_SVariateurs_BA1_CdeAllume+l_uc_CompteurBoitier]= 0;
						us_Tb_Echangeboitier[us_SVolets_BA1_Cdeouvre+l_uc_CompteurBoitier] = 0;
						us_Tb_Echangeboitier[us_SVolets_BA1_CdeFerme+l_uc_CompteurBoitier] = 0;
					}
				}
			}
			else
			{
				ul_EspionTachePrincipaleNbMessagesBAErreurs3++;
				DETECTION_ERREUR_TACHE_PRINCIPALE_MESSAGE_AVAILABLE;
				if(s_uc_FlagErreurDetectee == 0)
				{
					s_uc_FlagErreurDetectee = 1;
					vd_EspionRS_Printf(uc_ESPION_TACHE_PRINCIPALE_ERREUR,"Error pile de message pleine!\n");
				}
			}
		}
	}
}
