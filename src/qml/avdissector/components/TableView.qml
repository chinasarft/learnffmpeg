import QtQuick 2.9
import QtQuick.Controls 2.2

ListView {
    id: listView
    anchors.fill: parent

    contentWidth: headerItem.width
    flickableDirection: Flickable.HorizontalAndVerticalFlick
    property variant headers: []

    header: Row {
        spacing: 2
        function itemAt(index) { return repeater.itemAt(index) }

        Repeater {
            id: repeater
            model: headers
            Label {
                text: modelData
                font.bold: true
                font.pixelSize: 20
                // padding: 10
                background: Rectangle { color: "silver" }
                ToolSeparator {
                    anchors.left: parent.right
                    width: 2
                    height: itemAt(index).height
                    property real lastX: 0
                    background: Rectangle {
                        border.color: "#21be2b"
                    }
                    MouseArea {
                        anchors.fill: parent;
                        hoverEnabled: true;
                        cursorShape: Qt.SizeHorCursor
                        drag.target : parent
                        drag.minimumY: 0
                        drag.maximumY: 1000
                        drag.minimumX: 0
                        drag.maximumX: 1000
                        onPositionChanged: {
                            if(pressed) {
                                //console.log("index:"+index+ "|"+mouse.x+ " "+itemAt(index).width+" "+parent.lastX + " "+(mouse.x - parent.lastX), mouseX)
                                itemAt(index).width += (mouse.x - parent.lastX)
                                parent.lastX = mouse.x
                            }
                        }
                        onReleased: {
                            parent.lastX = 0
                        }
                        onPressed: {
                            console.log(mouseX, mouseY)
                        }

                    }
                }
                
            }
        }
    }

    model: 100
    delegate: Column {
        id: delegate
        property int row: index
        Row {
            spacing: 1
            Repeater {
                model: 5
                ItemDelegate {
                    property int column: index
                    text: qsTr("%1x%2").arg(delegate.row).arg(column)
                    width: listView.headerItem.itemAt(column).width
                }
            }
        }
        Rectangle {
            color: "silver"
            width: parent.width
            height: 1
        }
    }

    ScrollIndicator.horizontal: ScrollIndicator { }
    ScrollIndicator.vertical: ScrollIndicator { }
}
