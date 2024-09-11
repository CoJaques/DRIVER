.. _ssh:

#############################################
Mise en place de SSH sur la carte
#############################################

Cette page vous présente comment configurer les connexions SSH sur la carte.
Ceci permet d'avoir plusieurs terminaux ouverts simultanément,
ainsi qu'un meilleur support de ceux-ci (détection de la taille, ...)
Attention, il est possible que la connexion prenne un peu de temps !

Les deux méthodes, via mot de passe ou clé SSH sont présentées.
Seul l'une des deux méthodes a besoin d'être faite, de préférence avec les clés SSH.

==========================
Connexion par mot de passe
==========================

La méthode la plus simple est de configurer un mot de passe pour l'utilisateur root.
Pour cela, exécuter la commande :bash:`passwd` sur la carte (via l'UART) pour modifier
le mot de passe de l'utilisateur root.

Vérifiez également que l'option `PasswordAuthentication` dans le fichier `/etc/ssh/sshd_config`
soit mise à `yes` ou en commentaire. Si ce n'est pas le cas, modifier la ligne et redémarrer le service ssh :

.. code-block:: console

    $ service ssh restart

Après avoir choisi un mot de passe, vous pouvez vous connecter à l'aide de :

.. code-block:: console

    $ ssh root@192.168.0.2

=================
Connexion par clé
=================

Pour se connecter à l'aide d'une clé SSH,
il faut d'abord générer une clé SSH, si elle n'a pas déjà été créée, sur l'ordinateur hôte/VM à l'aide de :

.. code-block:: console

    $ ssh-keygen -t ecdsa

Vous pouvez généralement faire trois fois *enter* pour créer la clé.

La raison d'utiliser l'algorithme *ECDSA* est dû à un changement récent dans le client SSH qui requiert une version
plus sécurisée de l'algorithme *RSA* côté serveur qui n'est pas disponible sur la carte et ne permet pas donc pas
la connexion par défaut avec *RSA*. Utilisé l'algorithme *ECDSA* est le plus simple pour faire fonctionner avec
les versions récentes du client.
`Voir ici pour plus d'info <https://security.stackexchange.com/questions/270349/understanding-ssh-rsa-not-in-pubkeyacceptedalgorithms>`__

Ensuite, si la connexion par mot de passe a été activée, effectuez :

.. code-block:: console

    $ ssh-copy-id root@192.168.0.2

Si la connexion par mot de passe n'est pas activée, créez le fichier :file:`.ssh/authorized_keys` avec les bonnes permissions :

.. code-block:: console

    $ mkdir .ssh
    $ touch .ssh/authorized_keys
    $ chmod 700 .ssh
    $ chmod 600 .ssh/authorized_keys

Puis copiez le contenu du fichier :file:`~/.ssh/id_ecdsa.pub` sur l'hôte/VM
et coller dans le fichier :file:`~/.ssh/authorized_keys` sur la carte à l'aide de vi/vim ou nano.

Vous devriez pouvoir vous connecter à l'aide de :

.. code-block:: console

    $ ssh root@192.168.0.2
