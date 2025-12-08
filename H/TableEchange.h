// Version #3.0

// table de donn�es applicatives
// table d'�change avec l'�cran / table d'�change avec les boitiers / table d'�change Ethernet

// l�gende
// BP : boitier principal
// BA : boitier auxiliaire
// PDV : pi�ces de vie
// PDE : pi�ces d'eau
// CHB : chambres
// IHM : interface homme machine (l'�cran)

// mode automatique : programmation horaire sur 1 semaine
// 1 ordre pour 1 heure sur 7 jours : 6 modes possibles (4 bits) (cf consigne mode de chauffage)
// soit 84 octets 
#define uc_PLANNING_CHAUFFAGE_TAILLE	84

// arrosage 1 ordre pour 30 minutes sur 7 jours : 2 modes possibles (1 bit), soit 42 octets
#define uc_PLANNING_ARROSAGE_TAILLE		42

// nb variateurs possibles par boitier auxiliaire
#define uc_NB_VARIATEURS_POSSIBLES_PAR_BA	8

// nb lampes possibles par boitier auxiliaure
#define uc_NB_LAMPES_POSSIBLES_PAR_BA		16

// nb volets possibles par boitier auxiliaire
#define uc_NB_VOLETS_POSSIBLES_PAR_BA		8

// Cl� d'acc�s distance : 32 chiffres de 0 � 9 -> 16 octets
#define Cle_Acces_Distance_TAILLE			16	// !!!! Utilis� pour les adresses de stockage en EEPROM SERIE !!!!


// D�finition des param�tres de l'alarme
enum enumAlarmeConfig
{
	AlarmeConfig_Code,					// 1 = demander le code de l'alarme pour la mettre en route (0 : mise en route alarme sans code) - G�r� par �cran
	AlarmeConfig_Detect1,				// d�tecteur de pr�sence 1 d�sactiv� (0) ou utilis� (1)
	AlarmeConfig_Detect2,				// d�tecteur de pr�sence 2 d�sactiv� (0) ou utilis� (1)
	AlarmeConfig_DetectOuv,				// d�tecteur d'ouverture d�sactiv� (0) ou utilis� (1)
	AlarmeConfig_Detect1SurVoieAcces,	// d�tecteur de pr�sence 1 pas sur voie d'acc�s (0) ou sur voie d'acc�s (1)
	AlarmeConfig_Detect2SurVoieAcces,	// d�tecteur de pr�sence 2 pas sur voie d'acc�s (0) ou sur voie d'acc�s (1)
	AlarmeConfig_DetectOuvSurVoieAcces,	// d�tecteur d'ouverture pas sur voie d'acc�s (0) ou sur voie d'acc�s (1)
	AlarmeConfig_SireneInt,				// 0= ne rien faire, 1= sir�ne int�rieure activ�e
	AlarmeConfig_SireneExt,				// 0= ne rien faire, 1= sir�ne ext�rieure activ�e
	AlarmeConfig_BloqueVolets,			// 0= ne rien faire, 1 = Volets roulants bloqu�s en cas d'alarme
	AlarmeConfig_ForcerEclairage,		// 0= ne rien faire, 1 = Forcer allumage de toutes les �clairages
	
	AlarmeConfig_NB_VALEURS
};

// D�finition des param�tres pour un sc�nario
// Base Table d'Echange : 600 + valeur enum = indice final
enum enumScenario
{
	Scenario_Confirme_Scenario,		// 0 -> indice 600 - 1 = demander confirmation - G�r� par �cran
	
	// alarme
	Scenario_Alarme_ON,			// 1 -> indice 601 - 0 = ne rien faire, 1= mettre l'alarme, 2 = enlever l'alarme - G�r� par �cran
	Scenario_AlarmeConfig,		// 2 -> indice 602 - -> voir enumAlarmeConfig

	// �clairage - ETEINDRE
	Scenario_Eteindre_PDV_LSB = Scenario_AlarmeConfig + AlarmeConfig_NB_VALEURS, // 13 -> indice 613
							// 0  = ne rien faire / combinaison des valeurs suivantes pour �teindre les lampes voulues
							// 1  = �teindre la lampe de l'entr�e
							// 2  = �teindre la lampe du salon
							// 4  = �teindre la lampe du salon
							// 8  = �teindre la lampe du dressing 1
							// 16 = �teindre la lampe du dressing 2
	Scenario_Eteindre_PDV_MSB,	// 14 -> indice 614 - 0  = ne rien faire / combinaison des valeurs suivantes pour �teindre les lampes voulues 
							// 32 = �teindre le variateur du bureau
							// 64 = �teindre le variateur de la salle � manger
							// 128= �teindre le variateur du salon
	Scenario_Eteindre_CHB_LSB,	// 15 -> indice 615 - 0  = ne rien faire / combinaison des valeurs suivantes pour �teindre les lampes voulues
							// 1  = �teindre la lampe de l'escalier
							// 2  = �teindre la lampe 1 de la grande chambre
							// 4  = �teindre la lampe 2 de la grande chambre
							// 8  = �teindre la lampe 1 de la petite chambre 1
							// 16 = �teindre la lampe 2 de la petite chambre 1
							// 32 = �teindre la lampe de la petite chambre 2
							// 64 = �teindre la lampe de la petite chambre 3
	Scenario_Eteindre_CHB_MSB,	// 16 -> indice 616 - 0= ne rien faire / combinaison des valeurs suivantes pour �teindre les lampes voulues
							// 16 = �teindre le variateur de la petite chambre 3
							// 32 = �teindre le variateur de la petite chambre 2
							// 64 = �teindre le variateur de la petite chambre 1
							// 128= �teindre le variateur de la grande chambre
	Scenario_Eteindre_PDE_LSB,	// 17 -> indice 617 - 0= ne rien faire / combinaison des valeurs suivantes pour �teindre les lampes voulues
							// 1  = �teindre la lampe 1 de la cuisine
							// 2  = �teindre la lampe 2 de la cuisine
							// 4  = �teindre la lampe de la salle de bain 1
							// 8  = �teindre la lampe 1 de la salle de bain 2
							// 16  = �teindre la lampe 2 de la salle de bain 2
							// 32 = �teindre la lampe du WC 1
							// 64 = �teindre la lampe du WC 2
							// 128= �teindre la lampe de service
	Scenario_Eteindre_PDE_MSB,	// 18 -> indice 618 - 0= ne rien faire / combinaison des valeurs suivantes pour �teindre les lampes voulues
							// 1  = �teindre la lampe du d�gagement 1
							// 2  = �teindre la lampe du d�gagement 2
							// 4  = �teindre la lampe de la terrasse
							// 8  = �teindre la lampe 1 de l'annexe
							// 16 = �teindre la lampe 2 de l'annexe
							// 128= �teindre le variateur de la salle de bain 1
	// �clairage - ALLUMER
	Scenario_Allumer_PDV_LSB,	// 19 -> indice 619 - 0  = ne rien faire / combinaison des valeurs suivantes pour allumer les lampes voulues
							// 1  = allumer la lampe de l'entr�e
							// 2  = allumer la lampe 1 du salon
							// 4  = allumer la lampe 2 du salon
							// 8  = allumer la lampe du dressing 1
							// 16 = allumer la lampe du dressing 2
	Scenario_Allumer_PDV_MSB,	// 20 -> indice 620 - 0  = ne rien faire / combinaison des valeurs suivantes pour allumer les lampes voulues 
							// 32 = allumer le variateur du bureau
							// 64 = allumer le variateur de la salle � manger
							// 128= allumer le variateur du salon
	Scenario_Allumer_CHB_LSB,	// 21 -> indice 621 - 0  = ne rien faire / combinaison des valeurs suivantes pour allumer les lampes voulues
							// 1  = allumer la lampe de l'escalier
							// 2  = allumer la lampe 1 de la grande chambre
							// 4  = allumer la lampe 2 de la grande chambre
							// 8  = allumer la lampe 1 de la petite chambre 1
							// 16 = allumer la lampe 2 de la petite chambre 1
							// 32 = allumer la lampe de la petite chambre 2
							// 64 = allumer la lampe de la petite chambre 3
	Scenario_Allumer_CHB_MSB,	// 22 -> indice 622 - 0  = ne rien faire / combinaison des valeurs suivantes pour allumer les lampes voulues
							// 16 = allumer le variateur de la petite chambre 3
							// 32 = allumer le variateur de la petite chambre 2
							// 64 = allumer le variateur de la petite chambre 1
							// 128= allumer le variateur de la grande chambre
	Scenario_Allumer_PDE_LSB,	// 23 -> indice 623 - 0  = ne rien faire / combinaison des valeurs suivantes pour allumer les lampes voulues
							// 1  = allumer la lampe 1 de la cuisine
							// 2  = allumer la lampe 2 de la cuisine
							// 4  = allumer la lampe de la salle de bain 1
							// 8  = allumer la lampe 1 de la salle de bain 2
							// 16  = allumer la lampe 2 de la salle de bain 2
							// 32 = allumer la lampe du WC 1
							// 64 = allumer la lampe du WC 2
							// 128= allumer la lampe de service
	Scenario_Allumer_PDE_MSB,	// 24 -> indice 624 - 0  = ne rien faire / combinaison des valeurs suivantes pour allumer les lampes voulues
							// 1  = allumer la lampe du d�gagement 1
							// 2  = allumer la lampe du d�gagement 2
							// 4  = allumer la lampe de la terrasse
							// 8  = allumer la lampe 1 de l'annexe
							// 16 = allumer la lampe 2 de l'annexe
							// 128= allumer le variateur de la salle de bain 1
	// volets & store					
	Scenario_OuvrirVolets_PDV,	// 25 -> indice 625 - 0  = ne rien faire / combinaison des valeurs suivantes pour ouvrir les volets voulus
							// 1  = ouvrir le volet 1 du salon
							// 2  = ouvrir le volet 2 du salon
							// 4  = ouvrir le volet 3 du salon
							// 8  = ouvrir le volet 1 de la salle � manger
							// 16 = ouvrir le volet 2 de la salle � manger
							// 32 = ouvrir le volet du bureau
	Scenario_OuvrirVolets_CHB,	// 26 -> indice 626 - 0  = ne rien faire / combinaison des valeurs suivantes pour ouvrir les volets voulus
							// 1  = ouvrir le volet 1 de la grande chambre
							// 2  = ouvrir le volet 2 de la grande chambre
							// 4  = ouvrir le volet de la petite chambre 1
							// 8  = ouvrir le volet de la petite chambre 2
							// 16 = ouvrir le volet de la petite chambre 3
	Scenario_OuvrirVolets_PDE,	// 0  = ne rien faire / combinaison des valeurs suivantes pour ouvrir les volets voulus
							// 1  = ouvrir le volet 1 de la cuisine
							// 2  = ouvrir le volet 2 de la cuisine
							// 4  = ouvrir le volet de la salle de bain 1
							// 8  = remonter le store de la terrasse
	Scenario_FermerVolets_PDV,	// 0  = ne rien faire / combinaison des valeurs suivantes pour fermer les volets voulus
							// 1  = fermer le volet 1 du salon
							// 2  = fermer le volet 2 du salon
							// 4  = fermer le volet 3 du salon
							// 8  = fermer le volet 1 de la salle � manger
							// 16 = fermer le volet 2 de la salle � manger
							// 32 = fermer le volet du bureau
	Scenario_FermerVolets_CHB,	// 0  = ne rien faire / combinaison des valeurs suivantes pour fermer les volets voulus
							// 1  = fermer le volet 1 de la grande chambre
							// 2  = fermer le volet 2 de la grande chambre
							// 4  = fermer le volet de la petite chambre 1
							// 8  = fermer le volet de la petite chambre 2
							// 16 = fermer le volet de la petite chambre 3
	Scenario_FermerVolets_PDE,	// 0  = ne rien faire / combinaison des valeurs suivantes pour fermer les volets voulus
							// 1  = fermer le volet 1 de la cuisine
							// 2  = fermer le volet 2 de la cuisine
							// 4  = fermer le volet de la salle de bain 1
							// 8  = sortir le store de la terrasse
	// s�curit�
	Scenario_Securite,					// 0= ne rien faire, 1= couper les prises de s�curit�, 2= remettre les prises de s�curit�
	Scenario_Machines,					// 0= ne rien faire, 1= couper les machines � laver, 2= remettre les machines � laver
	
	// chauffage : consigne � appliquer imm�diatement
    // b0-b3 : consigne : 0 = OFF / 1 = CONFORT / 2 = ECO / 3 = ECO+ / 4 = ECO++ / 5 = HORS GEL
    // b4-b5 : mode : 0 = automatique / 1 = forc� / 2 = anticip�
    // b6 : 1 = Reprendre le dernier fonctionnement m�moris�
    // b7 : 1 = Continuer le fonctionnement actuel
    // :=> 0x00 � 0x05 := for�age en automatique (et suivi consigne en cours)
    // :=> 0x10 � 0x15 := for�age en mode forc� et consigne de for�age
    // :=> 0x20 � 0x25 := for�age en mode anticip� (et suivi consigne en cours)
    // :=> 0x40 := Reprendre le dernier fonctionnement m�moris�
    // :=> 0x80 := Continuer le fonctionnement actuel
	Scenario_Chauf_zj,					// consigne de chauffage zone jour
	Scenario_Chauf_zn,					// consigne de chauffage zone nuit
	Scenario_Chauf_zsb1,				// consigne de chauffage zone salle de bain 1
	Scenario_Chauf_zsb2,				// consigne de chauffage zone salle de bain 2
	
	// cumulus
	// 0x00 = For�age mode autonome (= HP permanentes) <- idem "Cumulus_Mode"
	// 0x01 = For�age gestion HP/HC <- idem "Cumulus_Mode"
	// 0x02 = For�age OFF (= HC permanent) <- idem "Cumulus_Mode"
	// 0x40 := Reprendre le dernier fonctionnement m�moris�
	// 0x80 := Continuer le fonctionnement actuel
	Scenario_Cumulus,
	
	// r�veil
	Scenario_Reveil_Reglage,			// 1 = d�rouler la proc�dure de r�glage des r�veils
	
	Scenario_Reveil_ON,					// 0 : ne rien faire � Utilis� par le sc�nario � je me l�ve �
										// 1 : armer la fonction r�veil � Utilis� par le sc�nario � je vais me coucher � si la fonction r�veil est activ�e
										// 2 : d�sactiver la fonction r�veil � Utilis� par le sc�nario � je vais me coucher � si la fonction r�veil est d�sactiv�e

	Scenario_Efface,					// 1= sc�nario � effacer (remise � 0 de tous les param�tres du sc�nario)
										// 2 � 6 = init par d�faut de sc�nario pr�d�fini
										// 0= ne rien faire / commande prise en compte par le BP
	Scenario_NB_VALEURS
};


enum Tbb_Donnees_Index
{
// ----------------					version					----------------
	Version_SoftBP_Embedded,						// Version soft BP (num�ro sauvegarde)
	Version_SoftBP_Web,								// Version Web (envoy�e au serveur pour gestion t�l�chargement - version de publication sur serveur)
	Version_SoftIHM_Majeur,							// Version soft IHM
	Version_SoftIHM_Mineur,							// Version soft IHM
	Version_TableEchange,							// version table de donn�es (pour v�rification compatibilit�)
	
// ----------------		heure / date du boitier principal	----------------
	Minutes,					// ! ne pas modifi� l'ordre de d�claration sans reprendre le code associ� !
	Heure,						// ! Minutes est utilis� comme indice de base !
	Jour,						// Mise a jour de ces champs en une fois sous MUTEX xxx
	Mois,
	Annee,
			
// ----------------				Etat du syst�me				----------------
	Status,			
		// bit 0 : 1= heures creuses en cours
		// bit 1 : 1= d�lestage en cours
		// bit 2 : 1= mode secouru
	Alerte,	
		// bit 0 : 1= d�clenchement alarme
		// bit 1 : 1= d�clenchement alerte suite � d�tection fuite d'eau lave linge
		// bit 2 : 1= d�clenchement alerte suite � d�tection fuite d'eau lave vaisselle
	Information,	
		// bit 0 : 1= d�faut com compteur ERDF
		// bit 1 : 1= d�faut com IHM
		// bit 2 : 1= d�faut com BA PDV 
		// bit 3 : 1= d�faut com BA CHB
		// bit 4 : 1= d�faut com BA PDE

// ----------------				chauffage					----------------
	// mode automatique : programmation horaire sur 1 semaine
	// 1 ordre pour 1 heure sur 7 jours : 6 modes possibles (4 bits)
	// b0-b3 : consigne : 0= OFF / 1= CONFORT / 2= ECO / 3= ECO+ / 4= ECO++ / 5= HORS GEL
	// soit 84 octets 
	Chauf_zj_Auto,														// chauffage zone jour
	Chauf_zn_Auto	= Chauf_zj_Auto + uc_PLANNING_CHAUFFAGE_TAILLE,		// chauffage zone nuit
	Chauf_zsb1_Auto	= Chauf_zn_Auto + uc_PLANNING_CHAUFFAGE_TAILLE,	 	// chauffage salle de bain 1
	Chauf_zsb2_Auto	= Chauf_zsb1_Auto + uc_PLANNING_CHAUFFAGE_TAILLE,	// chauffage salle de bain 2

	// chauffage : consigne � appliquer imm�diatement
    // b0-b3 : consigne : 0 = OFF / 1 = CONFORT / 2 = ECO / 3 = ECO+ / 4 = ECO++ / 5 = HORS GEL
    // b4-b5 : mode : 0 = automatique / 1 = forc� / 2 = anticip�
    // :=> 0x00 � 0x05 := for�age en automatique (et suivi consigne en cours)
    // :=> 0x10 � 0x15 := for�age en mode forc� et consigne de for�age
    // :=> 0x20 � 0x25 := for�age en mode anticip� (et suivi consigne en cours)
	Chauf_zj_Mode = Chauf_zsb2_Auto + uc_PLANNING_CHAUFFAGE_TAILLE,		// chauffage zone jour - PROTECTION PAR MUTEX
	Chauf_zn_Mode,														// chauffage zone nuit - PROTECTION PAR MUTEX
	Chauf_zsb1_Mode,													// chauffage salle de bain 1 - PROTECTION PAR MUTEX
	Chauf_zsb2_Mode,													// chauffage salle de bain 2 - PROTECTION PAR MUTEX
	// MUTEX : un nouvel ordre peut provenir de l'�cran pendant que BP est en train de traiter cet octet
	//         sans mutex, l'ordre risque d'�tre effac�
	//		   avec mutex, l'ordre de l'�cran sera pris en compte apr�s traitement donc pas de risque de perte...
	
// ----------------				Cumulus						----------------
	Cumulus_Mode,				// 0 = Autonome (ON) / 1 = gestion heures creuses / 2 = OFF

// ----------------					vacances				----------------
	// date et heure de fin de vacances : 
	VacanceFin_H,								// heure //xxx a proteger par mutex
	VacanceFin_Mn,								// minute 
	VacanceFin_J,								// jour 
	VacanceFin_M,								// mois 
	VacanceFin_A,								// ann�e 

	// chauffage fin de vacances : consigne forc�e
    // b0-b3 : consigne : 0 = OFF / 1 = CONFORT / 2 = ECO / 3 = ECO+ / 4 = ECO++ / 5 = HORS GEL
    // b4-b5 : mode : 0 = automatique / 1 = forc� / 2 = anticip�
    // b6 : 1 = Reprendre le dernier fonctionnement m�moris�
    // b7 : 1 = Continuer le fonctionnement actuel
    // :=> 0x00 � 0x05 := for�age en automatique (et suivi consigne en cours)
    // :=> 0x10 � 0x15 := for�age en mode forc� et consigne de for�age
    // :=> 0x20 � 0x25 := for�age en mode anticip� (et suivi consigne en cours)
    // :=> 0x40 := Reprendre le dernier fonctionnement m�moris� - NON GERE DANS VACANCE FIN !!! -> IDEM 0x80
    // :=> 0x80 := Continuer le fonctionnement actuel
	VacanceFin_zj_Force,						// chauffage zone jour
	VacanceFin_zn_Force,						// chauffage zone nuit
	VacanceFin_zsb1_Force,						// chauffage salle de bain 1
	VacanceFin_zsb2_Force,						// chauffage salle de bain 2
	
// ----------------					arrosage				----------------
	// s�lection du mode
	Arrose_Mode,			// 0 = OFF : pas d'arrosage
							// 1 � 254 : dur�e d'arrosage en mode forc�
							// 255 : mode automatique : pilotage en fonction du planning horaire
	// mode automatique : programmation horaire sur 1 semaine
	Arrose_Auto,			// 1 ordre pour 30 minutes sur 7 jours : 2 modes possibles (1 bit), soit 42 octets
	// d�tecteur de pluie
	Arrose_Detect= Arrose_Auto + uc_PLANNING_ARROSAGE_TAILLE,	// 0x00 := d�tecteur inactif (OFF) / 0x01 := d�tecteur utilis�			
	
// ----------------					alarme					----------------
	Alarme_AccesADistance,		// Autorise (1) ou non (0) la modification de l'�tat de l'alarme � distance - Renseign� par �cran (mis � 1 quand il passe en veille)
	Alarme_Mode,				// D�finie le mode de fonctionnement de l'alarme
								// 		0x00	Le syst�me d'alarme n'est pas utilis�
								//  	0x01	Mode r�glage
								//  	0x02	Alarme ind�pendante
								//		0x03	Alarme sur sc�nario "Je sors"
								//		0x04	Alarme sur sc�nario "Je vais me coucher"
								//		0x05	Alarme sur sc�nario "Je pars en vacances"
								//		0x06	Alarme sur sc�nario "Personnalis�"
	Alarme_Commande,			// 1 Demande de mise sous alarme -> repass� � 0 par BP apr�s prise en compte
	Alarme_CodeSaisiLSB,		// Bit 0-3 : 1er chiffre (le + � gauche � l'�cran) - Bit 4-7 : 2eme chiffre
	Alarme_CodeSaisiMSB,		// Bit 0-3 : 3eme chiffre - Bit 4-7 : 4eme chiffre (le + � droite � l'�cran)
								// Mettre / d�sactiver alarme si code LSB / MSB correct -> remis � 0xFFFF par BP apr�s prise en compte
	Alarme_Autorisation,		// 0x00 : traitement en attente
								// 0x01 : code valide
								// 0x02 : code invalide
	Alarme_SuiviAlarme,			// Suit le d�roulement des �tapes de l'automate	//xxx mettre en lecture seule !!!
								// 		0x00	Etape de d�part
								//  	0x01	Mise sous alarme impossible -> probl�me d'alimentation
								//  	0x02	Mise sous alarme impossible -> intrusion ou vandalisme
								//		0x03	Proc�dure de sortie
								//		0x04	R�gime de croisi�re
								//		0x05	Proc�dure d'entr�e
								//		0x06	Intrusion ou vandalisme
	Alarme_Detection,			// Par bit - 0 : pas de d�tection - 1 : d�tection	//xxx mettre en lecture seule
								//		bit 0 : �tat du d�tecteur d'ouverture
								//		bit 1 : �tat du d�tecteur pr�sence 1
								//		bit 2 : �tat du d�tecteur pr�sence 2
	Alarme_Fraude,				// Par bit - 0 : pas de fraude - 1 : fraude
								//		bit 0 : �tat de fraude tableau domotique
								//		bit 1 : �tat de fraude IHM
								//		bit 2 : �tat de fraude d�tecteur pr�sence 1
								//		bit 3 : �tat de fraude d�tecteur pr�sence 2
								//		bit 4 : �tat de fraude sir�ne d'int�rieur
								//		bit 5 : �tat de fraude sir�ne d'ext�rieur
								//		bit 6 : �tat batterie - 0 : pr�sente - 1 : non pr�sente
	Alarme_SuiviChangementCode,	// 0x00 : �tat de d�part
								// 0x01 : 1er envoi nouveau code
								// 0x02 : 2�me envoi nouveau code
	Alarme_CodeUser1LSB,		// Code alarme 1 Bit 0-3 : 1er chiffre (le + � gauche) - Bit 4-7 : 2eme chiffre
	Alarme_CodeUser1MSB,		// Code alarme 1 Bit 0-3 : 3eme chiffre - Bit 4-7 : 4eme chiffre (le + � droite)
	Alarme_CodeUser2LSB,		// Code alarme 2 Bit 0-3 : 1er chiffre (le + � gauche) - Bit 4-7 : 2eme chiffre						- NON UTILISE ACTUELLEMENT !
	Alarme_CodeUser2MSB,		// Code alarme 2 Bit 0-3 : 3eme chiffre - Bit 4-7 : 4eme chiffre (le + � droite)					- NON UTILISE ACTUELLEMENT !
	Alarme_CompteARebours,		// Temps restant en secondes en mode "proc�dure de sortie" ou "proc�dure de rentr�e"
	Alarme_Reserve,				// NOHN UTILISE ACTUELLEMENT
	Alarme_TestRAZPresence,		// Mode r�glage -> remis � 0 par BP apr�s prise en compte demande
								//		0x00 : ne fait rien
								//		0x01 : remet le bit du d�tecteur de pr�sence 1 du registre Alarme_Detection � "pas de d�tection"
								//		0x02 : remet le bit du d�tecteur de pr�sence 2 du registre Alarme_Detection � "pas de d�tection"
	Alarme_TestSirenes,			// Mode r�glage
								//		0x00 : Aucune sir�ne activ�e
								//		0x01 : Sir�ne d'int�rieur activ�e en alarme
								//		0x02 : Sir�ne d'ext�rieur activ�e en alarme

	AlarmeConfig,				// -> voir enumAlarmeConfig
	
// ----------------			alerte et s�curit�					----------------
	Alerte_Intensite = AlarmeConfig + AlarmeConfig_NB_VALEURS,	// reglage intensite de la sir�ne sur alerte : 25= fort / 50= moyen / 75= faible
	Alerte_Duree,					// reglage dur�e de l'alerte : 0= jusqu'� disparition / 1 � 255= temps en secondes
	Alerte_TestSirene,				// 1 = sir�ne � activer Intensit� alerte (activ� tant que cette info est non nulle)
	Alerte_Acquit,					// 1 = acquittement alerte xxx
	Securite_PriseCoupe,			// 1= prises de s�curit� coup�es, 0 = pilot�es
	Securite_FuiteLinge,			// 1= d�tection de fuite sur le lave-linge activ�e
	Securite_FuiteVaisselle,		// 1= d�tection de fuite sur le lave-vaisselle activ�e
	Securite_FuiteAlerte,			// 1= alerte sonore activ�e en cas de fuite d'eau
	
	// ----------------				r�veil					----------------
	// fonction r�veil : provoque l'ouverture des volants roulants
	// grande chambre
	Reveil_ChambreGr_H,		// heure
	Reveil_ChambreGr_Mn,	// minutes
	Reveil_ChambreGr_ON,	// 0 = fonction d�sactiv�e / > 0 = fonction active
	// chambre 1
	Reveil_Chambre1_H,		// heure 
	Reveil_Chambre1_Mn,		// minutes
	Reveil_Chambre1_ON,		// 0 = fonction d�sactiv�e / > 0 = fonction active
	// chambre 2
	Reveil_Chambre2_H,		// heure 
	Reveil_Chambre2_Mn,		// minutes 
	Reveil_Chambre2_ON,		// 0 = fonction d�sactiv�e / > 0 = fonction active
	// chambre 3
	Reveil_Chambre3_H,		// heure 
	Reveil_Chambre3_Mn,		// minutes 
	Reveil_Chambre3_ON,		// 0 = fonction d�sactiv�e / > 0 = fonction active
	// bureau
	Reveil_Bureau_H,		// heure 
	Reveil_Bureau_Mn,		// minutes
	Reveil_Bureau_ON,		// 0 = fonction d�sactiv�e / > 0 = fonction active
		
// ----------------				d�lestage					----------------
	Delestage,						// 0 = fonction d�sactiv�e / <>0 : fonction active

// ----------------		t�l�information du compteur ERDF	----------------
	TeleInf_OPTARIF,			// option tarifaire du compteur	
	TeleInf_PTEC,				// p�riode tarifaire en cours	
	TeleInf_ADPS,				// avertissement d�passement puissance souscrite	
	TeleInf_PAPP_LSB,			// puissance apparente utilis�e	(sur 2 octets)
	TeleInf_PAPP_MSB,
	// puissances consomm�es heures de base ou heures pleines //xxx a gerer
	TeleInf_HPB_Global_LSB,			// puissance globale
	TeleInf_HPB_Global_MSB,	
	TeleInf_HPB_Chauffage_LSB,		// chauffage
	TeleInf_HPB_Chauffage_MSB,
	TeleInf_HPB_Refroid_LSB,		// refroidissement
	TeleInf_HPB_Refroid_MSB,
	TeleInf_HPB_EauChaude_LSB,		// Eau chaude
	TeleInf_HPB_EauChaude_MSB,
	TeleInf_HPB_Prises_LSB,			// prises
	TeleInf_HPB_Prises_MSB,	
	TeleInf_HPB_Autres_LSB,			// autres
	TeleInf_HPB_Autres_MSB,	
	// puissances consomm�es heures creuses //xxx a gerer
	TeleInf_HC_Global_LSB,			// puissance globale
	TeleInf_HC_Global_MSB,	
	TeleInf_HC_Chauffage_LSB,		// chauffage
	TeleInf_HC_Chauffage_MSB,
	TeleInf_HC_Refroid_LSB,			// refroidissement
	TeleInf_HC_Refroid_MSB,	
	TeleInf_HC_EauChaude_LSB,		// Eau chaude
	TeleInf_HC_EauChaude_MSB,
	TeleInf_HC_Prises_LSB,			// prises
	TeleInf_HC_Prises_MSB,	
	TeleInf_HC_Autres_LSB,			// autres
	TeleInf_HC_Autres_MSB,	
	// puissances consomm�es : r�partition en % (param�trage)
	TeleInf_Repartition_Chauffage,	// chauffage
	TeleInf_Repartition_Refroid,	// refroidissement
	TeleInf_Repartition_EauChaude,	// Eau chaude
	TeleInf_Repartition_Prises,		// prises
	TeleInf_Repartition_Autres,		// autres

// ----------------				�clairage 					----------------
// configuration des variateurs : 0= TOR (avec rampe) / 1= gradateur / 2= TOR (sans rampe)
// Zone Variateurs_PDV_Conf -> Volets_PDE_Temps : infos BA envoy�es par tache et trait�e en "un bloc" -> doit rester compact - ne pas ins�rer de valeurs dans ce bloc
	Variateurs_PDV_Conf,								// 8 variateurs possibles par boitier
										//    salon
										// +1 salle � manger
										// +2 bureau
										// +3 � + 7 non utilis�
	Variateurs_CHB_Conf = Variateurs_PDV_Conf+uc_NB_VARIATEURS_POSSIBLES_PAR_BA,	
										//    grande chambre
										// +1 petite chambre 1
										// +2 petite chambre 2
										// +3 petite chambre 3
										// +4 � +7 non utilis�
	Variateurs_PDE_Conf = Variateurs_CHB_Conf+uc_NB_VARIATEURS_POSSIBLES_PAR_BA,		
										//    salle de bain 1
										// +1 � +7 non utilis�
// temps d'extinction (lampes simples) : 1 � 255 minutes / 0= pas d'extinction automatique
	Lampes_PDV_Temps = Variateurs_PDE_Conf+uc_NB_VARIATEURS_POSSIBLES_PAR_BA,	// 16 sorties simples possibles par boitier
										// non utilis�
	Lampes_CHB_Temps	= Lampes_PDV_Temps+uc_NB_LAMPES_POSSIBLES_PAR_BA,
										// non utilis�
	Lampes_PDE_Temps	= Lampes_CHB_Temps+uc_NB_LAMPES_POSSIBLES_PAR_BA,
										// +0 � + 4 non utilis�
										// +5 WC 1
										// +6 WC 2
										// +7 service 
										// +8 � + 15 non utilis�
	
	// ----------------			volets / store 				----------------
// temps d'action (volets, store) : 1 � 255 secondes (0 � 4 minutes)
	Volets_PDV_Temps	= Lampes_PDE_Temps + uc_NB_LAMPES_POSSIBLES_PAR_BA,		// 8 volets ou stores possibles par boitier
										//    volet salon
										// +0 volet salon
										// +1 volet salon
										// +2 volet salon
										// +3 volet salle � manger
										// +4 volet salle � manger
										// +5 volet bureau
										// +6 � + 7 non utilis�
	Volets_CHB_Temps	= Volets_PDV_Temps + uc_NB_VOLETS_POSSIBLES_PAR_BA,
										//    volet grande chambre
										// +1 volet grande chambre
										// +2 volet petite chambre 1
										// +3 volet petite chambre 2
										// +4 volet petite chambre 3
										// +5 � +7 non utilis�
	Volets_PDE_Temps	= Volets_CHB_Temps + uc_NB_VOLETS_POSSIBLES_PAR_BA,
										//    volet cuisine
										// +1 volet cuisine
										// +2 volet salle de bain 1
										// +3 store terrasse
										// +4 � +7 non utilis�

// ----------------				sc�narios					----------------
	Scenario = Volets_PDE_Temps + uc_NB_VOLETS_POSSIBLES_PAR_BA,	// num�ro du sc�nario � lancer
																	// 0 = aucun / commande prise en compte par le BP
	// Scenario1 = r�serv� au serveur Internet (ex: descendre les volets roulants / remonter le store)
	// Scenario2 = "Je sors" 
	// Scenario3 = "Je pars en vacances" 
	// Scenario4 = "Je rentre"
	// Scenario5 = "Je vais me coucher" 
	// Scenario6 = "Je me l�ve"
	// Scenario7 = "Personnalis� 1" 
	// Scenario8 = "Personnalis� 2"
	
	Scenario_DernierLance, 							// Dernier sc�nario lanc� - Utilis� par �cran pour connaitre le dernier sc�nario lanc� (en cas de remplacement de l'�cran)

	Scenario1,										// -> voir enumScenario
	Scenario2 = Scenario1 + Scenario_NB_VALEURS,	// -> voir enumScenario
	Scenario3 = Scenario2 + Scenario_NB_VALEURS,	// -> voir enumScenario
	Scenario4 = Scenario3 + Scenario_NB_VALEURS,	// -> voir enumScenario
	Scenario5 = Scenario4 + Scenario_NB_VALEURS,	// -> voir enumScenario
	Scenario6 = Scenario5 + Scenario_NB_VALEURS,	// -> voir enumScenario
	Scenario7 = Scenario6 + Scenario_NB_VALEURS,	// -> voir enumScenario
	Scenario8 = Scenario7 + Scenario_NB_VALEURS,	// -> voir enumScenario
	
	// Contiennent l'�tat du boitier principal (pour le serveur)
	EtatBP1 = Scenario8 + Scenario_NB_VALEURS,
		// bit 0 : alarme activ�e
		// bit 1 : alarme d�clench�e
		// Autres bits : � 0 (pour �volutions futures)
	EtatBP2,
		// A 0 (pour �volutions futures)
	
	Cle_Acces_Distance,	// 16 octets pour stocker la clef � 32 chiffres
						// Octet 0 / bits 0 � 3 = chiffre #1
						// Octet 0 / bits 4 � 7 = chiffre #2
						// ...
						// Octet 15 / bits 0 � 3 = chiffre #31
						// Octet 15 / bits 4 � 7 = chiffre #32
						// Etat initial : tout � 0x00
	
	// ----------------                  store                      ----------------

	Store_VR = Cle_Acces_Distance + Cle_Acces_Distance_TAILLE, // Ce registre ne sert que pour l'IHM.
	                                            // 0x00 = Store utilis� comme store.
	                                            // 0x01 = Store utilis� comme 15�me VR.
	                                            // ETAT INITIAL = 0x00.
	Store_Vitesse_Vent_Repliage,
	                                            // 0d = Pas de repliage automatique. De 1d � 255d = Vitesse du vent en km/h qui fait replier le store automatiquement.
	                                            // ETAT INITIAL = 0x00.
	Store_Vitesse_Vent_Instantane,
	                                            // Vitesse du vent en km/h de 0d � 255d.               
	                                            // ETAT INITIAL = 0x00.

	// ----------------               Constructeur                  ----------------

	Constructeur_CodeLSB,
	                                            // Code constructeur Bit 0-3 : 1er chiffre (le + � gauche) - Bit 4-7 : 2eme chiffre                  
	                                            // ETAT INITIAL = 0x11
	Constructeur_CodeMSB,
	                                            // Code constructeur Bit 0-3 : 3eme chiffre - Bit 4-7 : 4eme chiffre (le + � droite)
	                                            // ETAT INITIAL = 0x19

	// ----------------                   Test                      ----------------

	Test_ETOR_1,// Read only
				// Etat au fil de l'eau des ETOR de la carte BP.
				// Bit 0 = d�tecteur d'ouverture
				// Bit 1 = d�tecteur de pr�sence 1 signal
				// Bit 2 = d�tecteur de pr�sence 1 fraude
				// Bit 3 = d�tecteur de pr�sence 2 signal
				// Bit 4 = d�tecteur de pr�sence 2 fraude
				// Bit 5 = sir�ne d'int�rieur fraude
				// Bit 6 = sir�ne d'ext�rieur fraude
	Test_ETOR_2,// Read only
				// Etat au fil de l'eau des ETOR de la carte BP.
				// Bit 0 = d�tecteur fuite lave linge (� voir si possible)
				// Bit 1 = d�tecteur fuite lave vaisselle (� voir si possible)
				// Bit 2 = d�tecteur de pluie
				// Bit 3 = d�tecteur de vent
				// Bit 4 = bouton magique
				// Bit 5 = d�tecteur d'ouverture tableau domotique

	EtatEthernet,//Read only
				// Bit 0 = Etat c�ble (0=OK / 1=HS)
				// Bit 1 = Etat DHCP (0=OK / 1=HS)
				// Bit 2 = Etat DNS (0=OK / 1=HS)
				// Bit 3 = Etat serveur (0=OK / 1=HS)
	 
	Mode_Test,
				// Emp�che la sauvegarde des param�tres en cas de coupure d�alim secteur.
				// 0 = Mode de fonctionnement normal.
				// 1 = Mode de fonctionnement de test.
	
	AdresseMAC_1,	// Adresse MAC du BP (6 octets)
	AdresseMAC_2,
	AdresseMAC_3,
	AdresseMAC_4,
	AdresseMAC_5,
	AdresseMAC_6,
	 
	Nb_Tbb_Donnees
};

// CONSTANTES TELEINFO
enum option_tarifaire {		// groupe OPTARIF -> TeleInf_OPTARIF
	uc_OPT_TARIF_NON_RENSEIGNE,
	uc_OPT_TARIF_TIME_OUT,
	uc_OPT_TARIF_BASE,
	uc_OPT_TARIF_HC,		// heures creuses / heures pleines
	uc_OPT_TARIF_EJP,		// EJP
	uc_OPT_TARIF_BBR		// Tempo
};

enum periode_tarifaire {	// groupe PTEC -> TeleInf_PTEC
	uc_TARIF_NON_RENSEIGNE,
	uc_TARIF_TIME_OUT,
	uc_TARIF_TH,		// toutes heures
	uc_TARIF_HC,		// heures creuses 
	uc_TARIF_HP,		// heures pleines
	uc_TARIF_HN,		// heures normales
	uc_TARIF_PM,		// heures de pointe
	uc_TARIF_HCJB,		// heures creuses jours bleus
	uc_TARIF_HCJW,		// heures creuses jours blancs
	uc_TARIF_HCJR,		// heures creuses jours rouges
	uc_TARIF_HPJB,		// heures pleines jours bleus
	uc_TARIF_HPJW,		// heures pleines jours blancs
	uc_TARIF_HPJR		// heures pleines jours rouges
};

// groupe PAPP -> TeleInf_PAPP_LSB / TeleInf_PAPP_MSB
#define uc_PUISSANCE_APPARENTE_TIME_OUT	0xFF

// groupe HCHC / HCHP -> TeleInf_HPB_Global_LSB / TeleInf_HPB_Global_MSB / TeleInf_HC_Global_LSB / TeleInf_HC_Global_MSB
#define uc_HCHC_HCHP_TIME_OUT_BYTE	0xFF
#define uc_HCHC_HCHP_TIME_OUT_SHORT	0xFFFF
