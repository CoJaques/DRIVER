.. _laboratoire4:

#########################################################
Laboratoire 4 --- Développement de drivers kernel-space I
#########################################################

.. only:: html

   .. figure:: ../images/logo_drv.png
      :width: 6cm
      :align: right

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center


.. raw:: latex

    \newpage

=========
Objectifs
=========

* Savoir créer des drivers simples
* Comprendre les outils disponibles pour effectuer des transferts entre le user-space et le kernel-space

===================
Matériel nécessaire
===================

Vous avez juste besoin de l'archive disponible dans le git du laboratoire.

=====================
Structure d'un module
=====================

Un module noyau minimaliste peut être très simple, comme on peut le voir dans les
sources du module qui suit (*empty_module*) :

.. literalinclude:: empty.c
   :language: c

Chaque module doit obligatoirement déclarer une fonction d'initialisation, appelée
lors du :c:`insmod`, et une fonction de cleanup, appelée lors du :c:`rmmod`.
Le nom donné à ces fonctions est libre, mais celles-ci doivent être déclarées comme
étant les points d'entrée et de sortie du module via les fonctions
:c:`module_init` et :c:`module_exit`. :c:`__init` et :c:`__exit` sont des
attributs du compilateur qui permettent certaines optimisations.
Notez que ce module n'expose pas de numéros de majeur/mineur, ni de fonctions de
lecture/écriture, spécifiques aux périphériques de type char ou de type block.
Il ne peut donc pas être accédé par un device node.

La déclaration de l'auteur et de la licence n'est pas strictement nécessaire,
mais importante dans certains cas.
Vous trouverez davantage de détails sur ce dernier point `dans cet article <https://lwn.net/Articles/82305/>`__.

printk() & co
=============

Le mécanisme le plus "primitif" pour afficher des messages d'erreur dans le noyau
est la fonction :c:`printk()`.

Utilisée dans un :c:`printk`, la constante :c:`KERN_ERR` permet de préciser que le message
que l'on désire afficher est de type "erreur".
Il existe en tout 8 catégories de messages différentes :

0. :c:`KERN_EMERG` (système inutilisable)
1. :c:`KERN_ALERT` (action immédiate requise)
2. :c:`KERN_CRIT` (conditions critiques)
3. :c:`KERN_ERR` (erreur)
4. :c:`KERN_WARNING` (attention)
5. :c:`KERN_NOTICE` (condition normale, mais significative)
6. :c:`KERN_INFO` (information)
7. :c:`KERN_DEBUG` (informations de debug)

Ces niveaux de sévérité permettent de classifier les différents messages en provenance du noyau.
Ces niveaux sont aussi utilisés pour décider quels messages seront affichés dans la console et quels
autres seront juste enregistrés dans le kernel log.
Ce "filtrage" est appelé **loglevel**.
Le fichier :file:`/proc/sys/kernel/printk` permet de connaître le loglevel actuel du système.

.. code-block:: console

   $ cat /proc/sys/kernel/printk
   4	4	1	7

Le loglevel de la console (premier nombre) indique que toutes les erreurs (c.-à-.d. WARNING,
ERR, CRIT, ALERT et EMERG) seront affichés à l'écran.
Le deuxième nombre indique que tous les :c:`printk` sans loglevel spécifié seront de type 4 (WARNING),
le troisième nombre indique qu'une console ne peut pas avoir un loglevel inférieur à 1
(les messages de type KERN_EMERG sont toujours affichés) et le dernier nombre indique le loglevel
au boot (tous les messages sauf KERN_DEBUG sont affichés).
Notez que, peu importe le loglevel, tous les messages noyau sont enregistrés dans le log,
accessible par :bash:`dmesg`.
Plus d'infos dans `la documentation <https://www.kernel.org/doc/html/latest/core-api/printk-basics.html>`__.

Le loglevel par défaut de la console courante est modifiable par root :

.. code-block:: console

   $ echo 8 | sudo tee /proc/sys/kernel/printk
   $ cat /proc/sys/kernel/printk
   8	4	1	7

Notez que les messages noyau ne sont jamais affichés dans un terminal de type GUI (*xterm*, *konsole*, ...).
Ils sont affichés sur les terminaux de type tty ou série uniquement (utilisez *Ctrl+Alt+F[3..6]* pour basculer dans
un de ces terminaux, *Ctrl+Alt+F[1..2]* pointent sur le serveur graphique).

Alternativement, il est possible d'utiliser les alias :c:`pr_*()` (p.ex. :c:`pr_info()`) qui sont plus *joli*.
Voir la documentation pour les alias des différents niveaux de log.

Lors que l'on travaille avec un device, il y a la possibilité d'utiliser les fonctions de la famille :c:`dev_*()` (p.ex :c:`dev_info()`).
Ces fonctions prennent, en plus du message, un pointeur vers :c:`struct device`, généralement créé en amont via le device tree ou par un driver de bus (USB, PCI, ...) qui découvre le périphérique.
Cela permet aux fonctions de préfixer le message avec des informations provenant du device. Par exemple, pour le driver du dernier exercice, les messages sont préfixés par :c:`drv-lab4 ff200000.drv2024:`

====================================
Transfert de données user <-> kernel
====================================

Le noyau et l'espace utilisateur possèdent deux espaces d'adressage distincts.
Par exemple, lorsqu'un logiciel user-space effectue un appel système pour lire depuis un dispositif, le pointeur
contenant l'adresse à laquelle copier les données contient une adresse valide **uniquement** dans le contexte
d'exécution du programme dans l'espace utilisateur.
Il est donc **nécessaire** d'effectuer une traduction d'adresse ; on utilise pour cela la fonction
:c:`copy_to_user` plutôt qu'un simple :c:`memcpy`.
La fonction permettant un transfert de données dans l'autre sens est logiquement appelée :c:`copy_from_user`.

Pour transférer des types simples, comme des entiers ou des caractères, une autre paire de fonctions plus légères
existent, :c:`put_user` et :c:`get_user`.

D'autres fonctions aidant à ces transferts user <-> kernel existent :
:c:`access_ok` est une fonction de vérification d'adresse, :c:`strnlen_user` et :c:`strncpy_from_user` sont des
fonctions servant à gérer les transferts de tampons terminé par un byte nul.

La gestion des espaces mémoire dans le noyau Linux est expliquée en détail dans
`cet article <https://developer.ibm.com/articles/l-kernel-memory-access/>`__.

===============================
Allocation dynamique de mémoire
===============================

Dans le noyau, il est possible d'allouer dynamiquement de la mémoire à l'aide de fonctions
similaires à :c:`malloc` et :c:`free`:
:c:`kmalloc` et :c:`kfree` (et leurs variantes, p.ex., :c:`kzalloc`).
Ces fonctions se comportent comme leurs homologues de l'espace utilisateur, hormis le fait
que :c:`kmalloc` est plus finement paramétrable, possédant un argument supplémentaire obligatoire.

.. hint::

    Pour plus d'infos : chapitre 8 de `Linux Device Drivers, 3rd edition <https://lwn.net/Kernel/LDD3>`__.

.. admonition:: **Exercice 1 : read/write et allocation dynamique**

    Le module :c:`stack` dans le dossier :c:`stack_module` crée un *character device* accessible via le fichier :c:`/dev/stack`.
    Ce module doit agir comme une pile d'entier sur 32 bits.

    Compléter les fonctions :c:`stack_read` et :c:`stack_write`
    du module pour que le module se comporte de la façon suivante :

    * Lors de l'écriture d'un entier de 32 bits dans :c:`/dev/stack`, la donnée est ajoutée (PUSH) sur la pile
    * Lors de la lecture sur le fichier, le dernier entier ayant été ajouté sur la pile est retourné et enlever de la pile (POP)
    * Si un tableau d'entier est écrit, tous les éléments du tableau doivent être ajoutés dans la pile en commençant par le premier.
    * Si un tableau d'entier est lu, celui-ci doit être rempli au maximum en fonction du nombre de valeurs dans la pile.
    * Si le pile est vide lors de la lecture, le buffer user est intouché et la fonction retourne 0.
    * Si la taille lue/écrite est anormale (= pas un multiple de 4), une erreur est retournée.
    * Il n'y a pas de taille limite maximale à la pile.
    * L'implémentation de la pile est libre (tableau, liste, autre).
    * L'empreinte mémoire doit être minimal.
      P.ex. pour un tableau, la taille allouée correspond, au maximum, à la puissance de 2 supérieure au nombre d'éléments dans la pile.
      (C.-à-d. pour 10 éléments, au maximum 16 alloués, pour 1025 éléments au maximum 2048 éléments, ...)

    Le programme :file:`stack_test.c` implémente un test basique du driver.

Dans un module, la mémoire à laquelle nous avons accès est réservée au noyau.
Cependant, il s'agit encore, comme dans l'espace utilisateur, de mémoire paginée dans un espace
d'adressage virtuel.
Si l'on désire accéder à l'adressage physique d'un périphérique, un appel à la fonction
:c:`ioremap` s'avère nécessaire.
Cette fonction permet de mapper un intervalle d'adresses physique dans l'espace d'adressage du noyau.
Une fois la conversion effectuée, il existe plusieurs fonctions permettant de transférer des données
entre les deux domaines : :c:`iowrite`/:c:`ioread` pour les types simples, et :c:`memcpy_fromio`/:c:`memcpy_toio`
pour les tampons en chaînes de caractères.

.. warning::

    Même si vous avez des pointeurs, l'accès à la mémoire du dispositif
    **ne se fait qu'avec les fonctions susmentionnées !!!**
    L'utilisation de l'indirection des pointeurs est **interdite!!!**.
    Vous **devez** utiliser :c:`iowrite`/:c:`ioread`.

==========================
Managed resources
==========================

Lorsqu'on manipule certaines ressources, p. ex. la mémoire, on doit souvent
répéter des étapes "fixes" : dans le cas de la mémoire, on essaye de l'allouer,
et si tout va bien on doit se souvenir de la désallouer dès qu'on a terminé de
l'utiliser ou bien si une erreur est survenue entre-temps.
Bien sûr, un jour ou l'autre on va oublier de le faire, non ?

Pour éviter cela, le noyau offre les "managed resources".
Celles-ci sont utilisables en ajoutant le préfixe :c:`devm_` aux différentes fonctions
(:c:`kmalloc()` devient :c:`devm_kmalloc()`) et en ajoutant en premier paramètre le pointeur
vers :c:`struct device` (de la même manière que les fonctions :c:`dev_*()` pour le logging).
En utilisant ces fonctions, les ressources allouées seront automatiquement libérées lorsqu'une erreur
intervient à l'initialisation du module ou qu'il est déchargé. Il n'est donc plus nécessaire d'appeler
les fonctions inverses (:c:`devm_kfree()`) explicitement !
Attention à s'assurer de quand même libérer la mémoire quand elle n'est plus utilisée !

Cette fonctionnalité n'est pas disponible avec toutes les fonctions de gestion de ressources.
Plus d'information, ainsi qu'une liste de ces fonctions est disponible
dans `la documentation <https://www.kernel.org/doc/html/latest/driver-api/driver-model/devres.html>`__
et dans `cet article <https://lwn.net/Articles/222860>`__.

=============
Interruptions
=============

Le noyau Linux expose un mécanisme simple pour la gestion des interruptions.
Un appel à :c:`[devm_]request_irq()` permet d'enregistrer une fonction comme routine
pour un IRQ donné et :c:`[devm_]free_irq()` permet de libérer le numéro d'IRQ.

Le paramètre :c:`flags` peut combiner plusieurs valeurs de `cette liste <https://elixir.bootlin.com/linux/v6.8.6/source/include/linux/interrupt.h#L39>`__
dont par exemple **IRQF_SHARED** qui indique que le numéro d'IRQ peut être partagé entre plusieurs périphériques.

Notez que dans le hardware récent, x86 surtout, les numéros d'IRQ sont souvent partagés par
plusieurs périphériques.
Si l'exclusion mutuelle est requise pour la routine de service, il est aussi possible de
désactiver les interruptions sur la ligne IRQ courante en appelant
:c:`disable_irq` ou :c:`disable_irq_nosync`.

.. hint:: Pour plus d'infos : chapitre 10 de `Linux Device Drivers, 3rd edition <https://lwn.net/Kernel/LDD3>`__.

=================================================
Données privées, platform drivers, et device tree
=================================================

Les drivers n'existent pas dans un vacuum, mais ils sont généralement inclus dans
un sous-système.
Par exemple, les drivers des périphériques sur bus I2C seront dans le système I2C,
ceux sur PCIe dans le sous-système PCI, ...
Utiliser l'interface offerte par le sous-système permet de disposer d'une série
de fonctionnalités communes aux périphériques du même type.

Les **platform drivers**, en particulier, sont utilisés lorsqu'on a un type de
périphérique qui n'est pas *discoverable*, ils sont donc très utilisés dans le monde embedded.
Un périphérique est dit *discoverable* si le bus auquel il est connecté possède
une méthode pour prendre connaissance des différents périphériques qui y sont connectés.
Par exemple, lorsqu'on rajoute une carte PCIe, au démarrage le driver du bus va la détecter automatiquement
et charger le bon sous-driver. Par contre, le contrôleur du bus PCIe étant directement *memmory-mapped*,
il faut donner *manuellement* les informations au kernel pour le retrouver (hard-codé, via le device tree, ...).

Malgré les points en commun, chaque driver à des exigences particulières. Par
exemple, il a besoin de plusieurs pointeurs vers la mémoire, ou bien deux
numéros d'IRQ.
Pour garder ces informations *privées* au driver et, en même temps, pouvoir les
passer entre les différents appels de fonction sans avoir des interfaces
superflues, Linux a introduit la notion de **données privées**.
Il s'agit d'une structure de données qui peut être récupérée depuis certains
paramètres (avec une façon qui change, malheureusement, d'un sous-système à
l'autre). Observer l'utilisation des :c:`struct priv` dans les drivers :c:`reds_adder`
provenant des tutos.

.. admonition:: **Exercice 2 : Interrupt et platform drivers**

    Le dossier :file:`switch_copy_module` contient un squelette de module utilisant un platform driver.

    Compléter les différentes fonctions et la structure privée pour que lors de l'appui sur :c:`KEY0`,
    la valeur des switchs soit affichée sur les 7-segments et à l'appui sur :c:`KEY1` la valeur affichée soit incrémenté.

    Référez-vous aux codes du tutoriel pour les différentes étapes à faire dans les fonctions
    (allocation mémoire, configuration I/O et IRQ, ...)
    ainsi qu'à l'exercice 4 du :ref:`laboratoire2` pour l'utilisation des IRQ avec les boutons,
    notamment pour les différents registres et leurs utilités.

=========================================
Travail à rendre et critères d'évaluation
=========================================

.. include:: ../consigne_rendu.rst.inc
