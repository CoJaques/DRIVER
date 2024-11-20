.. _laboratoire5:

##########################################################
Laboratoire 5 --- Développement de drivers kernel-space II
##########################################################

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

* Savoir gérer threads, timers et interruptions en kernel-space
* Savoir utiliser les queues en kernel-space
* Apprendre à utiliser les symboles exportés par d'autres modules
* Savoir utiliser les mécanismes pour la configuration depuis le user-space (*command line parameters*, *sysfs*, ...)
* Connaître les mécanismes principaux de synchronisation

===================
Matériel nécessaire
===================

Ce laboratoire utilise le même environnement que le laboratoire précédent,
qui peut donc être réutilisé.

==================
KThreads et timers
==================

D'autres fonctionnalités que l'on retrouve également dans l'espace noyau sont les threads et les timers.
Il est possible pour un module de démarrer des tâches d'arrière-plan, d'effectuer un polling
sur un périphérique à intervalle régulier, ou encore de déléguer le traitement d'une interruption
à un thread afin de ne pas rester dans une routine d'interruption trop longtemps.

La macro :c:`kthread_run` permet de créer un kthread et de la démarrer automatiquement.
En *arrière-plan*, cette macro fait appel à :c:`kthread_create`, qui crée le kthread sans le démarrer et :c:`wake_up_process` qui le démarre.
Ces deux fonctions peuvent également être utilisées séparément en fonction des besoins.

Un kthread peut ensuite se terminer de deux manières différentes :

* de manière *spontanée*, en faisant un simple :c:`return`
* lorsque :c:`kthread_stop` a été appelé depuis un autre thread.

:c:`kthread_stop` ne tue pas le thread directement, mais active un flag partagé indiquant que le thread
doit s'arrêter et bloque en attendant que le thread se termine.
Ce flag est accessible comme retour de la fonction :c:`kthread_should_stop`, et le thread doit contrôler
périodiquement cette valeur s'il s'attend à être stoppé.
La valeur de retour de :c:`kthread_stop` correspond à la valeur de retour de la fonction.

Un thread peut être mis en sommeil grâce à la macro :c:`wait_event` (et ses variantes), qui prend comme
paramètre une queue sur laquelle le thread attend et une condition qui doit être respectée pour que
ce thread se réveille.
Le thread peut ensuite être réveillé via un appel à :c:`wake_up` (et ses variantes), prenant comme paramètre
la queue de threads à réveiller.
Vous pouvez trouver une description de ces fonctions
`ici <https://www.kernel.org/doc/html/latest/driver-api/basics.html#wait-queues-and-wake-events>`__.
Sur le même principe, il est également possible d'utiliser :c:`wait_for_completion` et :c:`complete`
donc la documentation est disponible `ici <https://www.kernel.org/doc/html/latest/scheduler/completion.html>`

Un module peut également faire appel aux timers s'il désire effectuer une tâche cyclique ou compter
précisément un intervalle de temps.
L'interface des timers a beaucoup changé, et il est assez difficile de trouver des exemples qui fonctionnent
(la plupart ne compilent même pas !). Vous pouvez trouver une description de la nouvelle interface
`ici <https://lwn.net/Articles/735887/>`__.

Des exemples d'utilisation de kthread et des timer sont disponibles dans le dossier :file:`example` dans les sources du labos.

.. warning::

    Attention, ces deux modules écrivent régulièrement dans les logs du kernel !
    Ne les laissez pas insérer trop longtemps, au risque de petit à petit remplir votre espace disque !

===============
Queues (KFIFOs)
===============

Un type de structure de données très souvent utilisé lorsque l'on discute avec un
vrai dispositif est la queue, aussi appelée **KFIFO**.
Le noyau offre une interface qui vous permet facilement d'insérer et de retirer
des éléments.
L'interface est détaillée `ici <https://www.kernel.org/doc/html/latest/core-api/kernel-api.html#fifo-buffer>`__, et
des exemples d'utilisation sont disponibles dans le sous-répertoire :file:`samples` des sources
du noyau.

.. admonition:: **Exercice 1 : KFIFOs et kthread**

    Implémentez un module du noyau qui permet la gestion d'une liste de lecture de musique.

    Une musique sera représentée par les informations suivantes :

    * Durée en seconde.
    * Titre (limiter à 25 caractères)
    * Nom de l'artiste/groupe (Limiter à 25 caractères)

    Le driver doit exposer le *device node* :c:`/dev/drivify`.

    Il doit être possible d'écrire dans ce fichier les informations d'une musique qui sera alors ajoutée dans la liste de lecture.
    Une seule écriture doit permettre le passage de toutes les informations d'une musique et la durée doit être sous forme d'un entier 32 bits.
    Les détails exacts pour la communication entre l'espace kernel et utilisateur sont laissés libres, le plus simple étant de définir une structure commune.

    La liste de lecture est limitée à 16 musiques au maximum.

    La lecture du *device node* en fait rien.

    La musique est ensuite contrôlable à l'aide des boutons :

    * :c:`KEY0` : Play/Pause (par défaut la musique est en pause)
    * :c:`KEY1` : Remise à zéro de l'avancée de la musique actuelle (l'état play/pause est conservé après la remise à zéro)
    * :c:`KEY2` : Passer à la musique suivante

    Les 7-segment affiches en continus le temps déjà écouté de la musique en cours (:c:`HEX3-2` pour les minutes et :c:`HEX1-0` pour les secondes).
    Si la liste de lecture est vide, l'affichage doit montrer :c:`00:00`.

    Lorsque la lecture d'une musique est en cours, la :c:`LED9` s'allume et doit s'éteindre lors de la mise en pause.
    Les :c:`LED4-0` affiche le nombre de musiques actuellement dans la liste de lecture (celle en cours de lecture comprise)
    au format binaire.

    Quand une musique se termine, la suivante est alors lancée. S'il n'y a plus de musique, la lecture se met en pause
    (c.-à-d. que lors du prochain ajout d'une musique, il faudra appuyer sur :c:`KEY0` pour la jouer).

    Ecrivez une application user-space permettant l'ajout de musique (à choix, à l'aide d'un tableau prédéfini ou via les arguments de l'application)

    .. warning::

        Contraintes pour l'exercice :

        * La liste de lecture utilise une :c:`kfifo`
        * Un :c:`kthread` est utilisé pour la gestion de l'affichage 7-segments et de la musique (temps restant, passage à la suivante)
        * Un :c:`timer` donne le *rythme* au thread

          * Le thread est mis en attente et, chaque seconde, il se fait réveiller par le :c:`timer`.
          * Le :c:`timer` doit tourner uniquement quand une musique est en cours de lecture.

    .. hint::

        Ce module sera complété dans les prochains exercices, mais un seul fichier est demandé à la fin.
        Libre à vous de regarder les prochains exercices et de tout implémenter directement ou d'y aller
        exercice par exercice. P.ex. la gestion des accès concurrents sera à ajouter dans un autre exercice.

=====
Sysfs
=====

*Once upon a time...* une interface permettait aux utilisateurs d'interagir
avec le noyau Linux.
Le nom de cette interface était **procfs**, et consistait en un filesystem virtuel avec lequel
le noyau pouvait montrer au user-space des informations sur son fonctionnement et il pouvait
récupérer des paramètres de configuration.
Malheureusement, avec le temps ce filesystem est devenu de plus en plus encombré par des
informations non pertinentes, mais la *"peur de casser quelque chose"*
empêchait de faire le ménage.
Ainsi, il a été décidé de repartir de zéro, mais d'une façon plus structurée, avec **sysfs**,
tout en gardant procfs pour le coeur du noyau.
En tant que développeur driver, vous êtes donc censés utiliser sysfs !!

Le répertoire du dispositif reds-adder v1.1 (voir :ref:`tutoriel2`) dans :file:`/sys` existe déjà
(grâce à l'appel à
:c:`misc_register()`). On peut le voir dans :file:`/sys/class/misc/` (car il
s'agit d'un miscellaneous device, c.-à-d., il appartient au miscellaneous
framework). En regardant les liens symboliques dans ce répertoire, on voit qu'il
est aussi enregistré en tant que platform device (et donc il a son propre
répertoire :file:`/sys/devices/platform/ff205000.reds-adder`).
Si l'on veut exposer des informations (soit pour nous aider dans le
debugging, soit pour nous permettre de configurer notre dispositif), on peut
créer des fichiers. Ces fichiers peuvent être de trois types :

* en lecture seule (:c:`DEVICE_ATTR_RO`)
* en écriture seule (:c:`DEVICE_ATTR_WO`)
* en lecture/écriture (:c:`DEVICE_ATTR_RW`)

Le noyau nous offre des macros pour nous aider dans la création de ces fichiers.
Par exemple, :c:`DEVICE_ATTR_RW(<name>)` permet d'instancier une structure :c:`device_attribute`
et de l'associer aux deux fonctions :c:`<name>_store()` (écriture) et :c:`<name>_show()` (lecture).
La macro :c:`DEVICE_ATTR()` permet d'instancier la structure de manière plus générique,
en donnant explicitement les permissions d'accès aux fichiers et les fonctions :c:`store` et :c:`show`.
D'autres macros sont également disponibles, `voir la documentation <https://www.kernel.org/doc/html/latest/driver-api/infrastructure.html#c.DEVICE_ATTR>`__

On n'aura ensuite qu'à appeler :c:`device_create_file()` en lui passant notre
dispositif et un pointeur à la structure :c:`dev_attr_<name>` créée par l'une des macros
ci-dessus, et on obtiendra notre fichier dans sysfs.
Cette procédure est présentée dans le driver reds-adder v2.1 (voir :ref:`tutoriel3`).

Dans la fonction :c:`..._store()`, on est censé lire une valeur venant l'utilisateur,
par exemple en utilisant la fonction :c:`kstrtoint()`
(voir `ici <https://www.kernel.org/doc/htmldocs/kernel-api/API-kstrtoint.html>`__)
ou avec :c:`sscanf()`.
Dans la fonction :c:`..._show()`, on retourne à l'utilisateur une valeur,
les fonctions :c:`sysfs_emit()` et :c:`sysfs_emit_at()`
(même fonctionnement que :c:`scnprintf()` voir `ici <https://www.kernel.org/doc/html/latest/filesystems/api-summary.html#c.sysfs_emit>`__)

Voici un exemple d'utilisation :

.. code-block:: c

		static ssize_t my_nice_name_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf,
						  size_t count)
		{
			int rc;
			struct priv *priv = dev_get_drvdata(dev);

			rc = kstrtoint(buf, 0, &priv->my_internal_variable);
			if (rc != 0) {
				rc = -EINVAL;
			} else {
				rc = count;
			}

			return rc;
		}

		static ssize_t my_nice_name_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
		{
			int rc;
			struct priv *priv = dev_get_drvdata(dev);

			rc = sysfs_emit(buf, 8, "%d\n", priv->my_internal_variable);

			return rc;
		}

		static DEVICE_ATTR_RW(my_nice_name);

		/* Then in the probe() function */
		/* ... */
		rc = device_create_file(&pdev->dev, &dev_attr_my_nice_name);
		/* ... */
		/* In the remove() function */
		/* ... */
		device_remove_file(&pdev->dev, &dev_attr_my_nice_name);
		/* ... */

Bien sûr, dans la fonction :c:`..._store()`, on pourrait aussi modifier le
comportement du dispositif, en ajoutant une :c:`iowrite32()` sur le dispositif...

L'entrée **sysfs** créée par cet exemple est disponible dans :file:`/sys/devices/platform/ff200000.drv2024/my_nice_name`
en utilisant le même platform device que les labos.

.. warning::

    sysfs a deux limitations :

    * on ne peut pas utiliser plus qu'une page de mémoire (normalement 4096 bytes)
    * idéalement un fichier sysfs = une valeur (plus de détail sur cette règle `dans cet article <https://lwn.net/Articles/378884/>`__)

    Si vous voulez sortir plus d'une valeur, il y a d'autres mécanismes
    prévus pour cela (p. ex., *debugfs*), mais qui vont au-delà des
    objectifs de ce cours...

.. admonition:: **Exercice 2 : sysfs**

    Ajoutez, à l'aide de :c:`sysfs`, les fonctionnalités suivantes dans le module de l'exercice précédent :

    * Récupérer du titre de la musique courante
    * Récupérer du nom de l'artiste de la musique courante
    * Récupérer du nombre de musiques dans la liste de lecture
    * Récupérer et changer l'état play/pause
    * Récupérer et changer le temps déjà passé de la musique actuelle
    * Récupérer la durée de la musique actuelle
    * Récupérer la durée totale de toutes les musiques de la liste de lecture

    Complétez simplement le fichier de l'exercice précédent, pas besoin d'en faire une copie.

===============
Synchronisation
===============

Le noyau Linux est un logiciel multithreadé et multiprocesseur qui est particulièrement sensible
aux problèmes de concurrence, car il doit, par sa nature, s'adapter aux événements générés par
le hardware sous-jacent.
Pour cette raison, il dispose d'un riche ensemble de primitives de synchronisation :

* mutex
* semaphores (déconseillés)
* spin locks
* refcounts
* ...

qu'on peut
voir dans le chapitre 5 de `Linux Device Drivers, 3rd edition <https://lwn.net/Kernel/LDD3>`__.

.. admonition:: **Exercice 3: synchronisation**

    Ajouter la gestion des accès concurrents aux différentes variables de votre module.
    Une seule contrainte, la variable permettant la gestion de l'état play/pause doit être atomique.

    Complétez simplement le fichier, pas besoin d'en faire une copie.

=========================================
Travail à rendre et critères d'évaluation
=========================================

.. include:: ../consigne_rendu.rst.inc
