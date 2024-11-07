.. _tutoriel2:

##############################################
Tutoriel 2 ---  REDS-adder driver v0.1 et v1.1
##############################################

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center

Dans ce tutoriel on explorera le développement de drivers Linux plus en détail.
En particulier, deux exemples de driver Linux ont été écrits pour notre dispositif
REDS-adder (voir :ref:`tutoriel1`), et un logiciel de test a également été développé.

Vous pouvez trouver ces fichiers dans le dépôt git.

Avec un peu d'imagination, on peut s'imaginer que notre dispositif REDS-adder soit
en réalité un puissant système de chiffrement, qui est censé fonctionner de la
façon suivante :

* l'utilisateur rentre la chaîne de caractères à chiffrer avec un appel à :c:`write()` sur le dispositif dans :file:`/dev`. Mais attention ! Notre dispositif de chiffrement n'opère qu'avec des entiers, il faudra donc bien soigner la conversion entre caractères et valeurs entières
* l'algorithme en effectue le chiffrement en rajoutant à la valeur ASCII de chaque caractère la valeur actuelle du registre :c:`VALUE`. La valeur du seuil ramène cette valeur à 1 grâce à l'intervention d'un interrupt
* en lisant depuis le fichier du dispositif dans :file:`/dev`, l'utilisateur obtient une chaîne d'entiers en format binaire, qui doit être convertie en chaîne de caractères pour qu'on puisse avoir notre message chiffré.

Afin que le dispositif soit détecté par le noyau, il est impératif de modifier le
device tree en rajoutant dans le fichier :file:`arch/arm/boot/dts/socfpga_cyclone5_sockit.dts`
(juste après le noeud de :c:`drv2024`) le noeud :

.. code-block:: c

    reds-adder {
    	compatible = "reds,reds-adder";
    	reg = <0xFF205000 0x1000>;
    	interrupts = <GIC_SPI 43 IRQ_TYPE_EDGE_RISING>;
    	interrupt-parent = <&intc>;
    };

Pourquoi 43 alors que l'interrupt que l'ingénieur HW nous a donné était 75 ?
Car il est shared, donc il faut soustraire 32 au numéro de l'interrupt, voir
`ici <http://billauer.co.il/blog/2012/08/irq-zynq-dts-cortex-a9/>`__.
(vous pouvez vérifier qu'il est correct en regardant la sortie de :bash:`cat /proc/interrupts` une fois
le driver inséré).

N'oubliez pas de compiler et copier le nouveau DT avec :

.. code-block:: console

   $ export TOOLCHAIN=/opt/toolchains/arm-linux-gnueabihf_6.4.1/bin/arm-linux-gnueabihf-
   $ make ARCH=arm CROSS_COMPILE=$TOOLCHAIN socfpga_cyclone5_sockit.dtb
   $ cp $HOME/tftpboot/socfpga.dtb $HOME/tftpboot/socfpga.dtb.old
   $ cp arch/arm/boot/dts/socfpga_cyclone5_sockit.dtb $HOME/tftpboot/socfpga.dtb

Les deux drivers d'exemples se situent dans les dossiers :file:`material/reds_adder__x.1` (`x` étant `0` et `1`).
Un fichier :bash:`Makefile` se trouve dans ces dossiers pour la compilation. Modifiez les valeurs des variables :bash:`KERNELDIR` (chemin vers la racine de votre copie local du kernel Linux cloné dans les précédents labos) et :bash:`TOOLCHAIN`.

Une fois une version du driver compilé à l'aide de :bash:`make`, un module (fichier :file:`.ko`) est créé, ainsi qu'un exécutable du programme de test.
Ces deux fichiers peuvent être copiés dans le dossier partagé avec la carte (:file:`/export/drv`).
Le module peut être inséré à l'aide de :bash:`insmod`. Attention à bien enlever le module avant d'insérer une nouvelle version à l'aide de :bash:`rmmod`.

Plus de détails sur les drivers sont donnés en tant que commentaires dans le code.
Explorer le code pour comprendre son fonctionnement et n'hésitez pas à vous en inspirer pour vos futurs drivers !
N'hésitez pas non plus à bricoler le logiciel de test ainsi que le driver !
