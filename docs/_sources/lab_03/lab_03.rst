.. _laboratoire3:

#######################################################
Laboratoire 3 --- Introduction aux drivers kernel-space
#######################################################

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

* Connaître les interfaces disponibles sous Linux pour communiquer avec un driver
* Connaître l'API de programmation des drivers
* Savoir créer des drivers simples

=========
Consignes
=========

Ce laboratoire consiste en une marche à suivre illustrée d'exemples.
À la fin de chaque section, un travail pratique sous forme de questions à
répondre ou de code à écrire doit être réalisé. Notez bien toutes vos étapes et
constatations dans votre rapport final.

===================
Matériel nécessaire
===================

Pour ce laboratoire vous avez besoin de la même configuration que pour le laboratoire 2 (tout
simplement on ne chargera pas le module pour les drivers user-space).

==========================================
Introductions aux périphériques sous Linux
==========================================

Linux définit plusieurs classes de périphériques :

* Les périphériques de type **char**, dans lesquels on lit et/ou
  écrit séquentiellement des données les unes après les autres, sans tampon,
  p.ex USB, cartes sons, claviers, touchscreen, RTC, etc,
* Les périphériques de type **block**, dans lesquels on peut lire ou écrire de
  manière adressée et aléatoire, avec des blocs de taille arbitraire :
  p.ex mémoire, disque, etc,
* Les périphériques réseau, qui n'obéissent pas aux interfaces des deux types
  précédents.

.. warning::

    Même pour des périphériques de type **char** les entrées/sorties
    peuvent être (et normalement sont) binaires ! Cette taxinomie concerne **uniquement**
    la façon dont ces dispositifs opèrent, et **non pas** les types de données
    qu'ils traitent !

Le support matériel pour ces périphériques est fourni par un driver sous forme
de module noyau. Un module peut être intégré au noyau à la compilation, mais il
est aussi possible de le compiler séparément en module insérable à la demande.
Ceci apporte comme avantage d'économiser la mémoire du noyau en libérant le
module lorsque le périphérique n'est pas utilisé. Notons que la compilation d'un
module séparé ne dispense pas de posséder une partie du code source du noyau
(les fichiers d'en-tête dans tous les cas).

Un périphérique sous Linux est considéré comme un fichier (voir
`Everything is a file <https://en.wikipedia.org/wiki/Everything_is_a_file>`__).

Le système offre la possibilité d’interagir avec lui via les mêmes appels
systèmes que ceux utilisés avec les fichiers conventionnels : :c:`read()`,
:c:`write()`.
Les périphériques de type char possèdent également certains appels
spéciaux, comme :c:`ioctl()` (*In-Out ConTroL*) qui permet de configurer le
périphérique.
Ces fonctionnalités, pour les drivers char et block, sont exposées sous forme de
fichier virtuel, appelé *device node* (ou *device file*), situé dans
:file:`/dev/`, Qui reçoit ces appels et les transmet au driver.
Pour identifier à quel driver est associé quel fichier virtuel, deux numéros
sont attribués à chacun de ces fichiers.
Ces numéros d'identification sont visibles dans la cinquième et sixième
colonne de la sortie de la commande :bash:`ls -la`.

.. code-block:: console

   HOST:~$ ls -la /dev
   total 4
   drwxr-xr-x  17 root     root          4440 Nov  1 07:13 .
   drwxr-xr-x  26 redsuser redsuser      4096 Sep 14 11:30 ..
   crw-------   1 root     root       10, 235 Nov  1 07:13 autofs
   ...
   crw-r-----   1 root     kmem        1,   1 Nov  1 07:13 mem
   ...
   crw-rw-rw-   1 root     root        1,   8 Nov  1 07:13 random
   ...
   crw-rw----   1 root     dialout   188,   0 Nov  1 07:13 ttyUSB0
   ...
   crw-rw-rw-   1 root     root        1,   9 Nov  1 07:13 urandom
   ...
   crw-rw-rw-   1 root     root        1,   5 Nov  1 07:13 zero

Le premier numéro (**majeur**) identifie de manière unique le driver, le
deuxième (**mineur**) identifie de manière unique le type de périphérique au
sein du driver. Par exemple, le majeur *1* correspond ici aux *memory devices* du
noyau : les périphériques :file:`kmem`, :file:`random`, :file:`urandom` et
:file:`zero` sont des périphériques virtuels intégrés au noyau.
Ces fichiers sont créés au démarrage par le gestionnaire de périphérique
(:file:`udev`, :file:`mdev`, ...) après l'énumération des bus, ou lors de
l'insertion d'un périphérique (hot-plug).
Il est également possible de créer manuellement des device nodes en
précisant le type et le numéro majeur et mineur via la commande :bash:`mknod`.

.. admonition:: **Exercice 1 : mknod**

    Utilisez la page de manuel de la commande :bash:`mknod` pour en comprendre le
    fonctionnement.
    Créez ensuite un fichier virtuel de type caractère avec le même couple
    majeur/mineur que le fichier :file:`/dev/random`.
    Qu'est-ce qui se passe lorsque vous lisez son contenu avec la commande
    :bash:`cat` ?

Exemple
=======

Le périphérique :file:`/dev/ttyUSB0` (sur la machine hôte) a un majeur 188 qui correspond à un matériel
bien réel : c'est le convertisseur USB/série de la DE1-SoC, visible sur votre
machine si le câble USB est branché au connecteur console de la carte.
Pour récupérer plus d'informations sur ce périphérique, les systèmes de fichiers
virtuels :file:`/proc` et :file:`/sys` sont utiles :
l'inspection du fichier :file:`/proc/devices`, listant les périphériques block
et char du système, permet de découvrir qu'il s'agit d'un périphérique de type
caractère.


.. admonition:: **Exercice 2 : proc**

    Retrouvez cette information dans le fichier :file:`/proc/devices`.

.. admonition:: **Exercice 3 : sysfs**

    :file:`sysfs` contient davantage d'informations sur le périphérique.
    Retrouvez-le dans l'arborescence de :file:`sysfs`, en particulier
    pour ce qui concerne le nom du driver utilisé et le type de connexion.
    Ensuite, utilisez la commande :bash:`lsmod` pour confirmer que le
    driver utilisé est bien celui identifié auparavant et cherchez si
    d'autres modules plus génériques sont impliqués.
    (en fonction de la distribution Linux, il se peut qu'aucuns modules
    plus génériques ne soient impliqués, car ils ont été configurés en mode "built-in"
    lors de la compilation du kernel)

======================
Compilation de modules
======================

En général, un driver simple se présente sous la forme d'un fichier C unique.
Dans l'arborescence du noyau, les drivers compatibles avec toutes les
architectures se situent dans le répertoire :file:`drivers` à la racine du code
source du noyau Linux.
Les sous-répertoires représentent les différentes familles de périphériques.
Le dossier :file:`char`, par exemple, contient tous les périphériques de type
char supportés, dont :file:`random` et :file:`zero` que nous avons vu auparavant.
Lors de la compilation du noyau et selon les options de compilation choisies,
les fichiers sources contenus dans ce répertoire seront inclus dans le noyau ou
pas.

Pour nos tests, il est plus simple de compiler un driver séparément. Pour faire
ceci, il est nécessaire d'avoir les sources du noyau pour lequel le driver doit
être compilé, et ces sources doivent déjà être configurées et un
:bash:`make modules` doit avoir été préalablement effectué (ce qui est déjà le cas
si les sources ont déjà été compilées une fois).

Si vous êtes sous une distribution de type Debian ou dérivée, pour compiler des
modules pour votre machine vous devez installer le paquet :file:`linux-headers`
correspondant à votre version du noyau (déjà installé sur la machine de
laboratoire).

.. code-block:: console

   $ sudo apt install linux-headers-$(uname -r)

Les sources déjà préparées seront installées dans le dossier
:file:`/lib/modules/$(uname -r)/build`.

Vous pouvez ensuite utiliser le :file:`Makefile` suivant pour compiler un module
(dans cet exemple, le module *empty*).

.. literalinclude:: ../../material/lab_03/Makefile.pc
   :language: Makefile

.. warning::

    *make* est très très strict au sujet des TABs. Vous trouverez ces
    Makefiles dans les fichiers du labo, merci de ne **pas**
    copier-coller les sources depuis le fichier HTML (car sinon *make*
    ne sera pas forcément enthousiaste de la chose...).

Si vous (cross-)compilez votre propre noyau, le répertoire à préciser dans le
:file:`Makefile` du module est simplement la racine du dossier contenant
l'arborescence du noyau.
Pour cross-compiler, il est donc nécessaire d'avoir les sources kernel et
d'indiquer le chemin.
Le :file:`Makefile` pour votre module *empty* à cross-compiler deviendra
donc :

.. literalinclude:: ../../material/lab_03/Makefile.de1soc
   :language: Makefile

C'est d'ailleurs tout ce qui a besoin d'être précisé dans le :file:`Makefile`,
hormis le nom du module, si l'on exclut les paramètres à donner à GCC.
Il est également possible de passer les paramètres de GCC à l'appel de make, ce
qui évite de devoir modifier le :file:`Makefile` :

.. code-block:: console

   $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

La cible *all* du :file:`Makefile` du module appelle la cible *modules* du
:file:`Makefile` du répertoire de base du noyau après avoir initialisé la
variable *obj-m* avec le nom du driver à compiler.

Une fois le module compilé, un fichier portant l'extension :file:`ko` est généré
dans le répertoire du module.
La commande :bash:`insmod` permet de charger le module dans le noyau.

.. warning::

    Les modules sont des parties sensibles du système.
    Ayant accès à l'espace mémoire du noyau, une erreur dans un module
    peut conduire **facilement** au crash du système.
    Comptez sur la possibilité d'un crash à chaque fois que vous
    insérez un de vos modules et sauvegardez vos fichiers !

.. hint::

    :bash:`strace` est un utilitaire permettant de lister tous les
    syscalls (appels à :c:`read()`/:c:`write()`/:c:`ioctl()` et autres)
    effectués par un processus.
    Cela est très utile pour contrôler les paramètres passés lors de la
    lecture/écriture/configuration de node devices.

La commande :bash:`rmmod` permet d'enlever un module.
Souvent, lors de l'insertion et du démontage du module, celui-ci affiche un
message d'information dans le log du noyau.
Ce log, stocké dans :file:`/var/log/kern.log`, peut être affiché directement via
la commande :bash:`dmesg`. L'option :bash:`-w` permet de `suivre` les nouveaux messages.
Si cette option n'est pas disponible (sur la carte p. ex.),
la commande :bash:`tail` permet de filtrer la fin d'un fichier, et avec la
commande :bash:`tail -f` vous pouvez observer en continu un fichier de texte pour
des lignes ajoutées à sa fin. (donc :bash:`tail - f /var/log/kern.log` étant plus ou moins équivalent à :bash:`dmesg -w`)

.. admonition:: **Exercice 4 : empty module**

    **Sur votre machine hôte (laptop, machine de labo), pas sur la DE1-SoC**.
    Compilez le module *empty* disponible dans les sources de ce
    laboratoire.
    Ensuite, montez-le dans le noyau, démontez-le, et analysez les
    messages enregistrés dans les logs.

Lorsqu'on désire insérer un module ayant été compilé avec le noyau, la commande
:bash:`modprobe` est préférée à la commande :bash:`insmod`, car elle va
directement chercher le module dans le bon sous-répertoire de
:file:`/lib/modules/` suivant la version du noyau démarré.
Cette commande prend comme paramètres le nom du module (pas le chemin complet
ni l'extension :file:`.ko`), et s'occupe également de résoudre les dépendances
entre modules en montant celles-ci avant le module demandé si nécessaire.

.. admonition:: **Exercice 5 : Parrot module**

    Le module *parrot* (disponible dans les sources de ce laboratoire), est
    un driver minimal de périphérique virtuel de type caractère.
    Il possède un majeur de 97.
    Une fois un **device node** associé à ce module, il est possible d'y écrire une chaîne de caractère et de la relire
    par la suite.
    A l'aide d'un appel à :c:`ioctl()` sur le device node, il est possible d'agir sur la chaîne en la mettant
    en majuscule/minuscule ou inversant la casse.

    Un appel :c:`ioctl()` prends deux paramètres entiers et renvoie un entier.
    Le programme :file:`ioctl.c` permet d'effectuer ces appels facilement.
    Afin d'utiliser les bonnes pratiques, les macros `_IO` et `_IOW` ont été utilisées pour générer la valeur du premier
    paramètre (= numéro de l':c:`ioctl`). Pour ce module, ces valeurs sont écrites dans les logs à l'insertion.
    Plus d'information sur ces macros sont disponibles `sur la doc officiel <https://www.kernel.org/doc/html/latest/userspace-api/ioctl/ioctl-number.html>`__
    ainsi que dans le chapitre 6 de `Linux Device Drivers, Third Edition <https://lwn.net/Kernel/LDD3/>`__ (dans les premières pages du PDF correspondant).

    Le module fonctionne de la façon suivante lors des appels :c:`ioctl` :

    * Si le premier paramètre vaux la valeur de :c:`PARROT_CMD_TOGGLE`, la casse des lettres est inversée (majuscule en minuscule et inverse)
    * Si le premier paramètre vaux la valeur de :c:`PARROT_CMD_ALLCASE`, toutes les lettres sont mises en majuscule/minuscule en fonction du second argument (:c:`TO_UPPERCASE` et :c:`TO_LOWERCASE`).

    Pour cet exercice :

    * Compilez et cross-compilez ce driver pour votre machine et pour la DE1-SoC.
      Vérifiez que le driver soit bien inséré sur les deux plates-formes
      et récupérez un maximum d'informations sur ce périphérique grâce aux
      outils précédemment vus.
    * Créez un device node afin de communiquer avec le driver (à choix sur votre machine ou sur la carte).
      Donnez les bons droits sur ce fichier afin que l'utilisateur courant puisse y accéder.
      Rendez un listing du device node. (c.-à-d. :bash:`ls -la /dev/mynode`).
    * Effectuez une écriture (:bash:`echo`) et une lecture (:bash:`cat`) sur ce device node.
      Grâce à :file:`ioctl.c`, testez la configuration du périphérique, puis démontez-le.
      Vérifiez également que le démontage du noyau ait bien été effectué.
      Rendez une copie texte de votre console.
    * Ce driver a été écrit par un ingénieur pressé qui n'a pas trop bien fait son boulot :
      les valeurs de retour ne sont pas contrôlées ! Aidez-le en rajoutant ces vérifications !
    * L'ingénieur qui a écrit ce driver doit se croire à l'âge de la pierre, car il a
      utilisé la fonction :c:`register_chrdev()` qui ne devrait pas être utilisée à
      partir du noyau 2.6...  Modernisez ce driver et rendez-le plus agréable à utiliser
      (p. ex., ce serait très bien de ne pas devoir créer à la main le fichier du dispositif...).
      Vous pouvez vous référer à `cet article <https://olegkutkov.me/2018/03/14/simple-linux-character-device-driver/>`__
      pour une utilisation de :c:`alloc_chrdev_region`, il est également possible d'utiliser :c:`register_chrdev_region`
      avec un major connu (généralement, on va préférer :c:`alloc` pour éviter d'avoir le même major qu'un autre driver).
      Il est également possible d'utiliser les misc device donc un exemple est donné par la V1 de :c:`reds_adder` dans les tutoriels.
    * Modifiez ensuite le module pour ajouter la commande :c:`ioctl` :c:`PARROT_CMD_RESET` qui permet de retourner à
      l'état des majuscules/minuscules originales (= valeur écrite par l'utilisateur).


=========================================
Travail à rendre et critères d'évaluation
=========================================

.. include:: ../consigne_rendu.rst.inc
