.. role:: bash(code)
   :language: bash

.. role:: c(code)
   :language: c

.. _tutoriel1:

############################
Tutoriel 1 --- Prise en main
############################

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center

Cette série de tutoriels a été développé afin de vous accompagner dans la création de drivers Linux.
On commencera par créer un driver pour un périphérique très simple (présenté ci-dessous), et on
évoluera vers des designs plus réalistes.
L'idée est de vous donner une méthodologie et du code "qui marche" (trop souvent le code que vous
trouvez sur Internet est bogué ou mal écrit), afin que vous puissiez être "inspirés" par ces codes.

.. warning::

    **Vous êtes invités à les bricoler jusqu'à ce qu'ils se cassent ! (les logiciels, pas les
    cartes !)**
    Rien n'est plus éducatif que bricoler quelque chose pour l'améliorer ou, tout
    simplement, essayer de comprendre comment ça fonctionne ;-)

=========
DRV-adder
=========

Le périphérique qu'on va vous décrire ne fait vraiment rien d'utile. Par contre, il représente un
bon exemple d'un périphérique mappé en mémoire --- son utilisation ne diffère pas beaucoup d'un
"vrai" dispositif sans avoir toute la complexité liée à un vrai dispositif, d'où son intérêt pour
nous.

Le développeur du périphérique nous a gentiment donné les adresses de registres :

+----------+-------------------------------+------+
|| Adresse || Registre                     | Type |
+==========+===============================+======+
|| 0x0000  || ID device (0xCAFECAFE)       | RO   |
+----------+-------------------------------+------+
|| 0x0004  || increment                    | RW   |
+----------+-------------------------------+------+
|| 0x0008  || value                        | RW   |
+----------+-------------------------------+------+
|| 0x000C  || initialization               | WO   |
+----------+-------------------------------+------+
|| 0x0010  || threshold                    | RW   |
+----------+-------------------------------+------+
|| 0x0080  || irq mask                     | RW   |
+----------+-------------------------------+------+
|| 0x0084  || irq capture                  | RW   |
+----------+-------------------------------+------+

(RO = Read Only, WO = Write Only, RW = Read-Write).

Il nous a aussi dit qu'il est parti du *DE1-SoC Computer System* fourni par
Altera.
Ci-dessous une capture d'écran qui montre les connexions dans QSys pour le Altera Computer,
avec tout au fond notre bloc (**reds_custom_0**) :

.. figure:: figures/qsys.png
   :width: 22cm
   :align: center

.. note::

    Explorez les connexions de notre bloc et essayez de découvrir pourquoi il a été ainsi câblé.

Depuis cette image, on peut s'imaginer que le dispositif aura un offset de 0xFF205000 dans la
mémoire (car les "lightweight FPGA slaves" sont mappés à partir de l'adresse 0xFF200000).
Le développeur du reds-adder nous a dit que le numéro d'interruption est 75.

.. note::

    Comment ai-je pu parvenir à cette valeur ? Pourquoi le designer n'a pas choisi 0xFF200000 ?
    Ou, encore mieux, 0x00000000 ?

Pour le vérifier, on peut essayer de lire l'ID du dispositif.

.. warning::

    Le développement d'un dispositif demande souvent l'étroite collaboration de personnes
    du côté HW et du côté SW de la force.
    Avoir un simple registre avec une constante facilement identifiable (non, zéro n'est pas
    une bonne valeur...) permet à l'ingénieur SW de vérifier que :

    * le dispositif est sous tension
    * le dispositif a une horloge active
    * il est en train de parler avec le bon dispositif
    * il sait correctement lire depuis le dispositif
    * l'endianness est celle qu'il pensait (ou pas) --- au moins, s'il a
      été malin lors du choix de la constante, donc peut-être 0xAAAAAAAA
      n'est pas la meilleure valeur non plus
    * ...

    Avoir en plus un registre avec le numéro de version du bitstream
    c'est top :)

    Votre assistant a bêtement passé des décennies de sa vie en essayant de communiquer avec
    des dispositifs en power-down, ou bien avec le dispositif à la mauvaise adresse, ou avec l'
    horloge désactivée, ou ...

    Vous êtes donc autorisés à frapper votre ingénieur HW jusqu'à ce qu'il accepte d'ajouter
    ces registres dans son design !!! ;-)

.. figure:: figures/reset.png
   :width: 22cm
   :align: center

Oups..... mais oui, c'est normal ! Il faut d'abord booter une fois la carte pour que l'image du
bitstream soit chargée dans la mémoire (faudrait vraiment faire ce qu'on demande dans le texte... non !?!?).

.. note::

    Est-il vraiment nécessaire ? Pourrait-on le faire depuis U-Boot sans
    booter Linux ? (regardez les commandes U-Boot utilisées pour booter le
    système avec la commande :console:`# printenv`)

Donc reboot, et notre hypothèse semble être confirmée :

.. figure:: figures/uboot_01.png
   :width: 12cm
   :align: center

On peut maintenant voir ce qu'il nous a communiqué au sujet du fonctionnement du dispositif :

* **value** et **threshold** sont initialisés à 0, **increment** à 1, les interrupts sont désactivés
  (**irq mask** à 0) et (bien sûr) il n'y a pas encore de interrupt capturés (donc **irq capture** est
  à 0)
* lorsque **increment** est à 1, une lecture de **value** en augmente la valeur de 1 et la retourne
* si **increment** est à 0 alors les lectures n'affectent pas **value**
* écrire 1 dans **initialization** remet la valeur de **value** à 0
* si **irq mask** est à 1, alors dès que **value** dépasse **threshold** un interrupt est levé (c.a.d,
  **irq capture** devient 1)
* pour nettoyer un interrupt, il faut écrire 1 dans **irq capture**.

.. note::

    Imaginons la situation suivante :

    * **value**       = 0x1a
    * **threshold**   = 0x1c
    * **increment**   = 0x01
    * **irq capture** = 0x01

    Qu'est-ce qui se passe avec 4 lectures consécutives ? Détaillez les valeurs de tous les
    registres. Ensuite, vérifiez vos réponses sur la carte.

Dans le prochain tutoriel on verra comment interagir avec ce dispositif depuis Linux, et ensuite on
en développera le driver.
