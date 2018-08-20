import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import "qrc:/components/"

Window {
    visible: true
    width: 640
    height: 480
    title: qsTr("Hello World")

    TableView {
        headers: ["Quisqssue", "Posuere", "Curabitur", "Vehicula", "Proin"]
    }

/*
    Item {
       width: 600
       height: 600

       //Model
       ListModel {
          id: objModel
          Component.onCompleted: {
              objModel.append({"name":"Zero","level":0,"subNode":[]})
              objModel.append({"name":"One","level":0,"subNode":[]})
              objModel.append({"name":"Two","level":0,"subNode":[]})
              objModel.get(1).subNode.append({"name":"Three","level":1,"subNode":[]})
              objModel.get(1).subNode.append({"name":"Four","level":1,"subNode":[]})
              objModel.get(1).subNode.get(0).subNode.append({"name":"Five","level":2,"subNode":[]})
          }
       }

       Component{
           id: headView
           Item {
               width: parent.width
               height: 30
               Row{
                   anchors.left: parent.left
                   anchors.verticalCenter: parent.verticalCenter
                   spacing: 8
                   Repeater {
                       model:["name", "level"]
                       Text {
                           text: modelData
                           font.bold: true
                           font.pixelSize: 20
                       }
                   }
               }
           }
       }//headview is end(定义的表头)

       //Delegate
       Component {
           id: objRecursiveDelegate
           Column {
               id: objRecursiveColumn
               MouseArea {
                   width: objRow.implicitWidth
                   height: objRow.implicitHeight
                   onClicked: {
                       for(var i = 1; i < parent.children.length - 1; ++i) {
                           parent.children[i].visible = !parent.children[i].visible
                       }
                   }
                   Row {
                       id: objRow
                       spacing: 8
                       Item {
                           height: 1
                           width: model.level * 20
                       }
                       Text {
                           text: (objRecursiveColumn.children.length > 2 ?
                                      objRecursiveColumn.children[1].visible ?
                                          qsTr("-  ") : qsTr("+ ") : qsTr("   ")) + model.name
                           color: objRecursiveColumn.children.length > 2 ? "blue" : "green"
                       }
                       Text {
                           text: model.level
                           color: "pink"
                       }
                   }

               }
               Repeater {
                   model: subNode
                   delegate: objRecursiveDelegate
               }
           }
       }

       //View
          ListView {
             header: headView
             anchors.fill: parent
             model: objModel
             delegate: objRecursiveDelegate
          }
    }
    */

}


