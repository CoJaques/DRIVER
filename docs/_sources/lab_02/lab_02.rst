.. _laboratoire2:

####################################
Laboratoire 2 --- User-space Drivers
####################################

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center

Le but de ce laboratoire est de vous présenter les drivers
user-space. Il s'agit d'une étape intermédiaire entre le monde user-space (que vous avez
exploré dans le :ref:`laboratoire1`) et le monde kernel-space
(qui sera le sujet du reste du cours).

Il n'est pas toujours nécessaire d'écrire un driver kernel-space, et opérer en
user-space présente des avantages intéressants (mais aussi de fortes limitations).
Dans ce laboratoire on explorera les deux.

.. raw:: latex

    \newpage

=========
Objectifs
=========

* Apprendre à effectuer des changements simples dans la configuration du noyau.
* Comprendre les différentes approches relatives aux drivers user-space.
* Investiguer les avantages/désavantages par rapport aux drivers kernel-space.

===================
Matériel nécessaire
===================

Dans ce laboratoire on mettra les mains dans le noyau !
Il nous faudra donc le télécharger.
On utilisera la version 6.1.55-lts du noyau Linux, mais on choisira une version non-mainline (c.-à-d., pas celle "officielle").
Ci-dessous, vous verrez comment récupérer le noyau, le configurer, et le compiler.


===============================================================================
Téléchargement du noyau Linux et ajout des modules pour le User-space I/O (UIO)
===============================================================================

Avant tout chose, sur la machine virtuelle, commencez par exécuter la commande suivante (et à chaque fois que vous ouvrez un nouveau terminal pour compiler du code kernel "manuellement") :

.. code-block:: console

    $ export TOOLCHAIN=/opt/toolchains/arm-linux-gnueabihf_6.4.1/bin/arm-linux-gnueabihf-

Comme mentionné ci-dessus, nous utiliserons une version du noyau non-mainline,
c.-à-d., la version customisée par une entité tierce (dans notre cas, Altera).
Vous aviez déjà cloné le dépôt git lors du :ref:`laboratoire0`.
Vérifiez que la branche de votre clone locale soit :bash:`socfpga-6.1.55-lts` qui est la version du noyau que nous utiliserons :

.. code-block:: console

    $ git branch --show-current
    socfpga-6.1.55-lts

Si ce n'est pas la bonne branche et comme l'option :bash:`--depth=1` avait été utilisée pour limiter la taille du clone,
le changement de branche *classique* n'est pas possible.
Le plus simple est de recloner le dépôt avec la bonne branche :

.. code-block:: console

    $ rm linux-socfpga -r
    $ git clone --depth=1 -b socfpga-6.1.55-lts https://github.com/altera-opensource/linux-socfpga.git
    $ cd linux-socfpga

Lorsqu'on doit configurer le noyau pour une nouvelle carte, ce n'est pas nécessaire de démarrer
de zéro à chaque fois.
En fait, des configurations de base ont été déjà préparées pour les cartes les plus connues.
Vous pouvez donc importer directement cette configuration comme point de départ :

.. code-block:: console

    $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN socfpga_defconfig

.. note::

    Pourquoi doit-on spécifier tous ces paramètres ?

Cette commande remplacera la configuration actuelle (contenue dans le fichier :file:`.config`) avec
une configuration adaptée à la carte.

.. warning::

    Cela veut aussi dire que, lorsqu'on veut jouer un peu avec la configuration sans trop de
    risques, il est sage de faire une copie du fichier :file:`.config` **avant** de la bricoler...
    même si automatiquement le *menuconfig* (qu'on verra ci-dessous) fait une copie
    de l'ancienne configuration (fichier :file:`.config.old`) à la sauvegarde.

Toutefois, cette configuration ne contient pas les options qui pourraient nous intéresser,
dans le cas spécifique de ce laboratoire les *userspace drivers* ne sont pas actifs.
Vous pouvez le voir avec la commande :

.. code-block:: console

    $ grep UIO .config

(*UIO = Userspace I/O*, vous trouvez l'howto officiel `ici <https://www.kernel.org/doc/html/latest/driver-api/uio-howto.html>`__).

Vous pouvez modifier la configuration avec :

.. code-block:: console

    $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN menuconfig

Dans l'interface ncurses, vous pouvez chercher les éléments d'intérêt avec la touche *"/"*.
Tapez *UIO* dans le champ de recherche, et avec les numéros sélectionnez le résultat qui vous
intéresse. Dans le cas de :numref:`menuconfig_search`, il s'agit du numéro *"1"*.

.. _menuconfig_search:
.. figure:: images/menuconfig_search.png
   :width: 800px
   :align: center

   Résultats de la recherche.

Avec la touche barre espace vous pouvez commuter entre *" "* (désélectionné), *"M"* (module),
et *"\*"* (inclus dans le noyau).
Choisissez de le compiler en tant que **module**.
Avec la touche entrée vous pouvez voir le sous-menu (:numref:`menuconfig_submenu`).

.. _menuconfig_submenu:
.. figure:: images/menuconfig_submenu.png
   :width: 800px
   :align: center

   Sub-menu UIO.

Ici sélectionnez *Userspace I/O platform driver with generic IRQ handling* pour
la compilation en tant que module.

.. note::

    Quelle est la différence entre ce driver et le *Userspace platform driver with generic irq and dynamic memory*?

Pour sortir du menu de configuration, avec les flèches et la touche entrée
choisissez *Exit* dans la barre en bas jusqu'au moment où le système vous demande
si vous voulez sauvegarder la nouvelle configuration.
Appuyez sur entrée pour le faire, et vous devriez être au nouveau de retour à la
ligne de commande.

Vérifiez que la nouvelle configuration a bien été sauvegardée :

.. code-block:: console

    $ grep UIO .config
    CONFIG_UIO=m
    # CONFIG_UIO_CIF is not set
    CONFIG_UIO_PDRV_GENIRQ=m
    # CONFIG_UIO_DMEM_GENIRQ is not set
    [...]

Il faut ensuite modifier le device tree pour qu'on puisse gérer les boutons avec le UIO driver.
Le device tree est une structure de donnée qui décrit les composants hardware (DRAM, UART, USB, bus I2C, ...) d'une carte
et permet au kernel de connaître et utiliser correctement ces différents composants,
notamment sur les différentes adresses mémoires, numéros d'interruption ou encore le nombre de coeur CPU disponible.
Ainsi, il n'y a pas besoin de hardcodé toutes ces informations !
Plus d'information sur les devices trees sont disponibles dans
`ces slides <https://bootlin.com/pub/conferences/2021/webinar/petazzoni-device-tree-101/petazzoni-device-tree-101.pdf>`__.


Ouvrez dans un éditeur de texte le fichier :file:`arch/arm/boot/dts/socfpga_cyclone5_sockit.dts`
et ajoutez, juste après la ligne qui contient le :c:`#include` (ligne 6) les deux lignes suivante, pour permettre l'utilisation de certain :c:`define` :

.. code-block:: c

    #include <dt-bindings/interrupt-controller/irq.h>
    #include <dt-bindings/interrupt-controller/arm-gic.h>

Ajoutez également, juste après la ligne qui contient le *compatible* (ligne 10-12), le bloc :

.. code-block:: c

    drv2024 {
        compatible = "drv2024";
        reg = <0xFF200000 0x1000>;
        interrupts = <GIC_SPI 41 IRQ_TYPE_EDGE_RISING>;
        interrupt-parent = <&intc>;
    };

.. note::

    Pouvez-vous expliquer les lignes ci-dessus ?

On peut maintenant compiler le noyau, les modules et le device tree.
(vous pouvez démarrer la compilation et ensuite lire le reste du texte, cela va
prendre un moment...)

.. code-block:: console

    $ # Compilation du noyau
    $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN -j6
    $ # Compilation des modules
    $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN modules
    $ # Copie des modules dans le sous-répertoire tmp/
    $ rm ./tmp -rf
    $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN INSTALL_MOD_PATH="./tmp" modules_install
    $ # Compilation du DT
    $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN socfpga_cyclone5_sockit.dtb

    $ # Backup de l'ancien noyau/DT et copie des nouveaux fichiers
    $ cp $HOME/tftpboot/socfpga.dtb $HOME/tftpboot/socfpga.dtb.old
    $ cp $HOME/tftpboot/zImage $HOME/tftpboot/zImage.old
    $ cp arch/arm/boot/dts/socfpga_cyclone5_sockit.dtb $HOME/tftpboot/socfpga.dtb
    $ cp arch/arm/boot/zImage $HOME/tftpboot/
    $ cp ./tmp/lib/modules/6.1.55-g57cf7f3b7f73-dirty/ /export/drv/ -R

.. warning::

    Le nom du répertoire source dans la dernière commande pourrait changer !!

.. note::

    En effet on aurait dû copier les modules déjà pour le :ref:`laboratoire1`,
    lorsqu'on a utilisé le nouveau noyau (qui se trouvait dans l'archive fournie),
    mais on ne les utilisait pas donc on a (sciemment) sauté une étape...

.. note::

    Qu'est-ce que ces commandes font ? Est-il nécessaire de recompiler tout
    le noyau suite à nos changements ? Et si l'on modifie encore le Device Tree?

Sur la carte :

.. code-block:: console

    root@de1soclinux:~# cd /lib/modules
    root@de1soclinux:~# mv /home/root/drv/6.1.55-g57cf7f3b7f73-dirty/ .

Redémarrez la carte.
Maintenant le noyau devrait avoir les drivers user-space disponibles comme
modules.
Vérifiez que ce soit effectivement le cas en tapant :

.. code-block:: console

   root@de1soclinux:~# zcat /proc/config.gz | grep UIO

.. note::

    Le noyau Linux contient des millions d'options configurables.
    N'hésitez pas à explorer :c:`menuconfig` pour vous faire une idée de ce qui est disponible.

=====================================================
Accès aux périphériques en utilisant :file:`/dev/mem`
=====================================================

Dans le :ref:`laboratoire1` vous avez vu comment accéder à des
dispositifs memory-mapped en utilisant :file:`/dev/mem`.

.. admonition:: **Exercice 1**

    Ecrivez un logiciel user-space qui permet de calculer une somme de nombre de la manière suivante :

      * Le somme courante est affichée sur les 7-segments en décimal
      * Les switches permettent de choisir un nombre en binaire
      * **KEY0** permet d'ajouter ce nombre à la somme courante
      * **KEY1** permet de remettre à zéro la somme
      * Si la somme dépasse 999'999 (valeur maximale affichable) la **LED0** doit s'allumer

    Utiliser :c:`mmap` et :file:`/dev/mem` pour accéder aux périphériques.

=====================================================
Accès aux périphériques en utilisant le UIO framework
=====================================================

L'approche utilisée au point précédent présente plusieurs soucis, notamment au
niveau de la sécurité du système.
Le Userspace I/O framework résout ce type de problème en permettant un accès
plus restreint à la mémoire.
En effet, il est possible de cibler (dans le device tree) exactement la région
de mémoire qu'on veut exposer à l'utilisateur.

Le device tree que vous avez bricolé fait cela -- il offre
au système l'accès à une région de 4096 bytes à partir de l'adresse
*0xFF200000*.

En effet, le comportement de :c:`mmap()`, dans le cas de UIO, est en partie
différent (voir, par exemple,
`ici <https://www.osadl.org/fileadmin/dam/rtlws/12/Koch.pdf>`__).

.. important::
    Pour pouvoir configurer le driver générique du UIO (uio_pdrv_genirq)
    correctement il faut qu'il soit chargé avec la bonne valeur du paramètre
    of_id.
    Il faut donc l'enlever de la mémoire et le charger à nouveau, en choisissant
    le dispositif qui doit être contrôlé.

    Cette commande doit être exécutée après chaque boot pour utiliser les UIO !

    .. code-block:: console

        root@de1soclinux:~# modprobe -r uio_pdrv_genirq; modprobe uio_pdrv_genirq of_id="drv2024"

.. admonition:: **Exercice 2**

    * Pourquoi une région de 4096 bytes et non pas 5000 ou 10000 ? Et
      pourquoi on a spécifié cette adresse ?
    * Quelles sont les différences dans le comportement de :c:`mmap()`
      susmentionnées ?
    * Effectuez des recherches avec Google/StackOverflow/... et résumez par écrit les
      avantages et les inconvénients des drivers user-space par rapport aux drivers
      kernel-space.

.. admonition:: **Exercice 3**

    À l'aide du UIO HOW-TO (disponible
    `ici <https://www.kernel.org/doc/html/latest/driver-api/uio-howto.html>`__) et
    du tutoriel disponible
    `ici <https://yurovsky.github.io/2014/10/10/linux-uio-gpio-interrupt.html>`__,
    écrivez un driver user-space pour réaliser la même tâche que
    l'exercice 1. (garder une copie de l'exercice 1)

    Pas besoin de gérer les interruptions pour cet exercice.

==============================
UIO framework et interruptions
==============================

Un autre très grand avantage du UIO framework par rapport à l'accès à travers
:file:`/dev/mem` est la possibilité de gérer les interruptions (bien que d'une
façon un peu "primitive").

.. admonition:: **Exercice 4**

    À l'aide du tutoriel disponible
    `ici <https://yurovsky.github.io/2014/10/10/linux-uio-gpio-interrupt.html>`__,
    améliorer le programme des exercices précédents (en gardant une copie de ceux-ci)
    de la façon suivante :

      * Utiliser les interruptions UIO pour la gestion des boutons
      * Ajouter la possibilité de faire les autres opérations mathématiques :

        * **KEY0** permet d'additionner
        * **KEY1** permet de soustraire
        * **KEY2** permet de multiplier
        * **KEY3** permet de diviser

      * La valeur des switches est interprétée comme un entier signé en suivant la représentation du complément à deux (**SW9** étant le bit de signe)
      * La **LED9** affiche le signe (éteinte pour positif et allumée pour négatif)
      * Le résultat est limité à l'intervalle affichable (de -999'999 à 999'999).

        * Si le résultat obtenu sort de cet intervalle, la **LED0** s'allume
        * Par exemple : :c:`(999'990 + 22) - 22 => 999'999 - 22 => 999'977`


    .. warning::

        Les interruptions **doivent** être activées et le registre
        **doit** être nettoyé après chaque interruption...
        Voir section 3.4 du document *DE1-SoC Computer System with ARM Cortex-A9* !!

    .. hint::

        Il faut d'abord être sûrs que les interruptions soient bien reçues.
        Le fichier :file:`/proc/interrupts` pourra vous aider.
        Vous pouvez par exemple utiliser la commande

        .. code-block:: console

            root@de1soclinux:~# cat /proc/interrupts

        pour observer les interruptions reçues par le système.
        Utilisez cette commande juste avant et juste après avoir lancer votre programme
        (ou un plus simple de test) et avoir appuyé sur des boutons.
        (le programme doit au moins activer les interruptions correctement pour qu'une interruption
        soit levée)

        Il est également possible d'utiliser :bash:`watch` pour voir les interruptions en direct,
        mais cette commande fonctionne mieux en *SSH*, qui permet d'avoir plusieurs terminaux
        et d'ajuster correctement la taille de ceux-ci (voir :ref:`ssh`) :

        .. code-block:: console

            root@de1soclinux:~# watch -n1 "cat /proc/interrupts"

.. admonition:: **Exercice 5**

    Il y a au moins 3 façons pour attendre une interruption au moyen de :file:`/dev/uio0` (voir dans le tutoriel). Lesquelles ?

    Écrivez une version du logiciel user-space de l'exercice 5 pour
    chacune de ces façons et détaillez par écrit leurs
    différences/avantages/inconvénients.

=========================================
Travail à rendre et critères d'évaluation
=========================================

Dans le cadre de ce laboratoire, vous devez rendre les 5 exercices ci-dessus.

.. include:: ../consigne_rendu.rst.inc
