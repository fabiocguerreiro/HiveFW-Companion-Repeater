# HiveFW Companion Repeater

**HiveFW** é um firmware personalizado baseado no [MeshCore](https://github.com/meshcore-dev/MeshCore), que expande o conceito de **Companion Radio** do MeshCore com um modo **Repeater** integrado, conectividade Wi-Fi e uma base para automação doméstica e serviços remotos.

O projeto foi concebido para **hardware compatível com MeshCore**, combinando comunicação LoRa em rede mesh com conectividade de rede local e plataformas de automação como o Home Assistant.

> **HiveFW = MeshCore Companion + Repeater + Wi-Fi + Automação**

---

## Visão Geral

A principal característica do HiveFW é simples:

> **O HiveFW é, primeiro e sempre, um MeshCore Companion Radio.**

A funcionalidade Companion é o núcleo do firmware e permanece disponível independentemente do estado do modo Repeater.

O **modo Repeater é uma funcionalidade adicional e opcional**, que pode ser ativada diretamente através da aplicação MeshCore.

Quando o utilizador ativa o modo Repeater pela aplicação, o HiveFW passa a disponibilizar automaticamente as funcionalidades adicionais específicas do Repeater, incluindo o comportamento de anúncios, o Smart Advert e o Node Discovery.

Em outras palavras:

> **Companion primeiro. Repeater quando ativado.**

Isto permite utilizar o mesmo firmware como um Companion Radio normal ou, quando necessário, ativar as capacidades de Repeater sem necessidade de instalar um firmware diferente.

```text
                    MeshCore App
                         │
                         │
                         ▼
                 ┌───────────────┐
                 │    HiveFW     │
                 │               │
                 │   Companion   │◄──── Sempre ativo
                 │      +        │
                 │   Repeater    │◄──── Ativado pela app
                 └───────┬───────┘
                         │
                  ┌──────┴──────┐
                  │             │
                 LoRa          Wi-Fi
                  │             │
                  ▼             ▼
             MeshCore       Serviços Remotos
               Mesh          Home Assistant
                              Automação
```

O objetivo é disponibilizar uma plataforma prática para **interação remota com MeshCore, telemetria, automação e futuras aplicações de bots por solicitação**, mantendo sempre a experiência Companion no centro do firmware.

---

## Principais Características

### Companion Primeiro

A característica fundamental do HiveFW é ser um **Companion Radio primeiro e sempre**.

* A funcionalidade normal de MeshCore Companion Radio continua a ser a base do firmware.
* A funcionalidade Repeater é opcional.
* O modo Repeater é ativado ou desativado através da aplicação MeshCore.
* Com o Repeater desativado, o equipamento funciona como um Companion Radio normal.
* Ao ativar o Repeater, as funcionalidades adicionais específicas do Repeater são ativadas.
* Não é necessário instalar um firmware diferente para alternar entre Companion normal e Companion + Repeater.

Desta forma, o HiveFW disponibiliza uma **única solução de firmware**, capaz de adaptar o equipamento à função pretendida pelo utilizador.

### Evolução do Companion

O desenvolvimento do Companion no HiveFW **não é considerado concluído**.

Estão previstas novas funcionalidades e melhorias para o modo Companion em futuras versões, com o objetivo de expandir as capacidades do equipamento e, ao mesmo tempo, manter a compatibilidade com o fluxo de utilização normal do MeshCore Companion.

---

### Companion + Repeater

Quando o modo Repeater é ativado através da aplicação MeshCore, o HiveFW ativa as funcionalidades adicionais necessárias ao funcionamento como Repeater.

Atualmente, estas incluem:

* Funcionalidade LoRa Repeater integrada.
* Anúncios específicos para funcionamento como Repeater.
* Smart Advert.
* Ciclo de anúncios de 23 horas.
* Agendamento determinístico dos anúncios.
* Pequena variação temporal (*jitter*) para reduzir anúncios simultâneos entre vários nós.
* Funcionalidade Node Discovery para operação como Repeater.

O equipamento não precisa, portanto, de estar permanentemente configurado como Repeater.

O utilizador pode continuar a utilizá-lo como um **Companion Radio normal** e ativar as funcionalidades adicionais de Repeater quando necessário.

---

### Conectividade Wi-Fi

A ligação Wi-Fi do HiveFW tem como principal objetivo permitir **acesso remoto e integração com serviços externos**.

Isto permite que um equipamento HiveFW funcione como uma ponte entre a rede LoRa MeshCore e aplicações ou serviços executados numa rede local ou remotamente.

A conectividade Wi-Fi cria uma base para integrações como:

* **Home Assistant**.
* [meshcore-ha](https://github.com/meshcore-dev/meshcore-ha).
* **meshcore-chat-ha**.
* Computadores e servidores.
* Interfaces MeshCore remotas.
* Serviços de rede locais.
* Sistemas de monitorização e automação.

O objetivo é permitir que um nó HiveFW permaneça permanentemente ligado à rede MeshCore e possa ser acedido ou integrado remotamente através de Wi-Fi, sem depender de uma ligação USB permanente.

---

### Smart Advert

O HiveFW inclui uma implementação personalizada de **Smart Advert** para a operação como Repeater.

A implementação atual inclui:

* Ciclo de anúncios de 23 horas.
* Agendamento determinístico dos anúncios.
* Pequena variação temporal para reduzir anúncios simultâneos entre vários nós.
* Comportamento de anúncios adaptado ao modo Repeater.

---

### Node Discovery

Está disponível funcionalidade básica de **Node Discovery** para a operação como Repeater.

A implementação atual está focada especificamente na funcionalidade Repeater e poderá ser expandida em futuras versões.

---

# Integração com Home Assistant

Um dos principais objetivos do HiveFW é criar uma ponte entre o **MeshCore e o Home Assistant**.

A ligação Wi-Fi permite que um equipamento HiveFW permanentemente ligado comunique com o Home Assistant através de projetos como o **meshcore-ha** e o **meshcore-chat-ha**.

Isto cria uma ponte entre a rede LoRa e os sistemas de automação doméstica, permitindo trocar remotamente informações e comandos selecionados.

Possíveis aplicações incluem:

* Consultar valores de sensores do Home Assistant.
* Informação ambiental e de temperatura.
* Estado de dispositivos e sistemas.
* Informação energética.
* Informação de presença.
* Telemetria remota.
* Acionar automações previamente definidas no Home Assistant.
* Enviar mensagens para canais MeshCore.
* Monitorizar canais MeshCore.
* Responder automaticamente a comandos explicitamente solicitados.

A intenção é manter esta interface leve e controlada, disponibilizando apenas a informação e as ações que forem explicitamente configuradas.

---

# Automação Remota e Bots MeshCore

O HiveFW está a ser desenvolvido tendo em conta **automação controlada e sob solicitação**.

Uma plataforma de automação ligada, como o Home Assistant, poderá fornecer respostas previamente definidas a comandos recebidos através da rede MeshCore.

Por exemplo:

```text
Nó MeshCore Remoto
        │
        │ comando / pedido
        ▼
   Rede LoRa Mesh
        │
        ▼
     HiveFW
        │
        │ Wi-Fi
        ▼
 Home Assistant
        │
        │ resposta predefinida
        ▼
     HiveFW
        │
        ▼
   Rede LoRa Mesh
        │
        ▼
Nó MeshCore Remoto
```

Possíveis aplicações incluem:

* Consultas remotas de sensores.
* Pedidos de estado do sistema.
* Notificações automáticas.
* Comandos remotos previamente definidos.
* Respostas a pedidos de ping.
* Serviços de telemetria.
* Bots MeshCore leves.
* Interfaces para automação doméstica.

### Comportamento Responsável dos Bots

Os bots e serviços automatizados devem funcionar **principalmente sob solicitação**, respondendo a pedidos explícitos em vez de gerar continuamente tráfego na rede.

O objetivo do HiveFW não é incentivar *flooding* ou a geração excessiva de tráfego na rede MeshCore.

As automações devem ser leves, úteis e, sempre que necessário, utilizar mecanismos de limitação de frequência (*rate limiting*).

A rede MeshCore é um **recurso rádio partilhado**. O tempo de utilização do canal, a largura de banda e os restantes recursos da rede são partilhados entre diferentes utilizadores e nós.

Por esse motivo, anúncios excessivos, mensagens automáticas, bots demasiado ativos ou pedidos repetidos podem consumir desnecessariamente tempo de antena e prejudicar a utilização da rede por outros participantes.

O princípio deve ser simples:

> **Automatizar quando necessário, responder quando solicitado e evitar tráfego desnecessário.**

---

# Configuração de Rádio

O HiveFW inclui atualmente presets de rádio MeshCore para Portugal, destinados especificamente à utilização em **modo Repeater**:

|  Frequência | Largura de Banda | SF | CR | Modo     |
| ----------: | ---------------: | -: | -: | -------- |
| 433.375 MHz |         62.5 kHz |  9 |  6 | Repeater |
| 869.618 MHz |         62.5 kHz |  7 |  6 | Repeater |

Estes presets destinam-se a instalações **Repeater em Portugal**.

Os parâmetros de rádio devem ser sempre selecionados e utilizados de acordo com a legislação aplicável, atribuições de frequência, limites de potência e restantes requisitos locais.

As definições de frequência podem ser adaptadas a outros países, regiões ou instalações locais.

---

# Hardware

O HiveFW foi desenvolvido para **hardware compatível com a arquitetura MeshCore**.

O projeto não está limitado a uma única placa ou fabricante. O suporte depende da existência da respetiva definição de hardware MeshCore e do ambiente de compilação PlatformIO correspondente.

### Hardware atualmente testado / direcionado

* Equipamentos compatíveis com MeshCore podem ser suportados através das respetivas definições de hardware MeshCore.

O repositório poderá incluir ambientes de compilação específicos para determinados equipamentos quando necessário.

---

# Compilação

O HiveFW utiliza **PlatformIO** para a compilação.

Clonar o repositório:

```bash
git clone https://github.com/fabiocguerreiro/HiveFW-Companion-Repeater.git
cd HiveFW-Companion-Repeater
```

Compilar o ambiente correspondente ao equipamento:

```bash
pio run -e <environment>
```

Os ambientes de compilação disponíveis podem variar consoante o hardware suportado e incluído no repositório.

---

# MeshCore

O HiveFW é baseado no [MeshCore](https://github.com/meshcore-dev/MeshCore), um projeto open-source de redes LoRa Mesh desenvolvido para comunicação de longo alcance e descentralizada.

O MeshCore fornece a base de comunicação utilizada pelo HiveFW, incluindo:

* Comunicação LoRa.
* Encaminhamento de pacotes através de múltiplos nós.
* Funcionalidade Companion Radio.
* Funcionalidade Repeater.
* Descoberta de nós.
* Telemetria e outras capacidades da rede Mesh.

O HiveFW desenvolve esta base com funcionalidades adicionais centradas em **conectividade Wi-Fi, operação como Repeater e integração com sistemas de automação doméstica**.

Para mais informações sobre o projeto base, consulte a [documentação do MeshCore](https://docs.meshcore.io/).

---

# Utilização Responsável e Disclaimer

O HiveFW é fornecido **"tal como está" e sem garantias**. A utilização do firmware, do hardware rádio, das funcionalidades de automação e dos serviços associados é feita **por conta e risco do utilizador**.

É responsabilidade do utilizador garantir que o equipamento e a configuração rádio utilizada estão em conformidade com a legislação e regulamentação aplicáveis.

O HiveFW destina-se a experimentação, projetos pessoais, redes MeshCore e automação responsável.

O HiveFW **não deve ser utilizado para gerar tráfego excessivo ou desnecessário**, provocar *flooding* contínuo da rede MeshCore, executar bots abusivos ou interferir deliberadamente com o funcionamento normal da rede.

A rede MeshCore é um **espaço rádio partilhado**. Todos os utilizadores e nós partilham o mesmo recurso de rádio e, por isso, devem utilizá-lo de forma responsável e respeitar os restantes participantes.

Bots e sistemas automatizados devem funcionar preferencialmente **sob solicitação**, responder apenas a pedidos legítimos e evitar transmissões repetitivas ou desnecessárias.

O objetivo é que a automação acrescente funcionalidades à rede sem prejudicar a sua utilização por outros membros da comunidade.

> **Use a rede como gostaria que os outros utilizassem a rede quando o seu nó está no ar: com respeito, moderação e bom senso.**

Ao utilizar o HiveFW, o utilizador reconhece que é responsável pela sua própria configuração, transmissões, automações e utilização da rede.

---

# Objetivos do Projeto

O HiveFW está a ser desenvolvido com vários objetivos de longo prazo:

* Expandir as funcionalidades Companion + Repeater.
* Melhorar a integração Wi-Fi.
* Disponibilizar uma interface sólida para Home Assistant.
* Integrar com projetos como **meshcore-ha** e **meshcore-chat-ha**.
* Permitir telemetria remota.
* Desenvolver funcionalidades de bots MeshCore leves e **sob solicitação**.
* Adicionar novas funcionalidades ao modo Companion.
* Disponibilizar interfaces de automação configuráveis.
* Suportar mais hardware compatível com MeshCore.
* Manter, sempre que possível, compatibilidade com o projeto MeshCore original.
* Promover uma utilização responsável e eficiente do tempo de antena LoRa.

O projeto pretende evoluir em conjunto com o ecossistema MeshCore.

---

# Versão

**HiveFW v1.17.1-hivefw**

---

# Créditos

O HiveFW é baseado no excelente trabalho do projeto **MeshCore** e dos seus contribuidores.

* [Repositório GitHub do MeshCore](https://github.com/meshcore-dev/MeshCore)
* [Documentação do MeshCore](https://docs.meshcore.io/)

O HiveFW acrescenta funcionalidades específicas do projeto, mantendo como base a arquitetura MeshCore.

---

# Licença

O HiveFW é baseado no MeshCore e mantém os termos de licenciamento aplicáveis ao projeto original.

Consulte o [repositório do MeshCore](https://github.com/meshcore-dev/MeshCore) e os ficheiros de licença incluídos neste repositório para obter a informação de licenciamento aplicável.
